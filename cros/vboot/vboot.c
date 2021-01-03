// SPDX-License-Identifier: GPL-2.0+
/*
 * General functions used by vboot implementation
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT
#define NEED_VB20_INTERNALS

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <mapmem.h>
#include <asm/io.h>
#include <cros_ec.h>
#include <cros/vboot.h>
#include <u-boot/crc.h>

int vboot_alloc(struct vboot_info **vbootp)
{
	gd->vboot = malloc(sizeof(struct vboot_info));
	if (!gd->vboot) {
		log_err("Cannot allocate vboot %x\n",
                        (uint)sizeof(struct vboot_info));
		return -ENOMEM;
	}
	memset(gd->vboot, '\0', sizeof(struct vboot_info));
	*vbootp = gd->vboot;

	return 0;
}

struct vboot_info *vboot_get(void)
{
	struct vboot_info *vboot = gd->vboot;

	return vboot->valid ? vboot : NULL;
}

struct vboot_info *vboot_get_alloc(void)
{
	struct vboot_info *vboot = gd->vboot;

	if (!vboot)
		vboot_alloc(&vboot);

	return vboot;
}

struct vboot_info *vboot_get_nocheck(void)
{
	return gd->vboot;
}

int vboot_load_config(struct vboot_info *vboot)
{
	ofnode node;

	node = cros_ofnode_config_node();
	if (!ofnode_valid(node))
		return -ENOENT;

	vboot->deactivate_tpm = ofnode_read_bool(node, "deactivate-tpm");
	vboot->disable_dev_on_rec = ofnode_read_bool(node,
						     "disable-dev-on-rec");
	vboot->ec_efs = ofnode_read_bool(node, "ec-efs");
	vboot->ec_slow_update = ofnode_read_bool(node, "ec-slow-update");
	vboot->ec_software_sync = ofnode_read_bool(node, "ec-software-sync");
	vboot->has_rec_mode_mrc = ofnode_read_bool(node, "recovery-mode-mrc");
	vboot->meminit_in_ro = ofnode_read_bool(node,
						"meminit-in-readonly-code");
	vboot->oprom_matters = ofnode_read_bool(node, "oprom-matters");
	vboot->physical_dev_switch = ofnode_read_bool(node,
						      "physical-dev-switch");
	vboot->physical_rec_switch = ofnode_read_bool(node,
						      "physical-rec-switch");
	vboot->resume_path_same_as_boot = ofnode_read_bool(node,
						"resume-path-same-as-boot");
#ifndef CONFIG_SPL_BUILD
	vboot->detachable_ui = ofnode_read_bool(node, "detachable-ui");
	vboot->disable_memwipe = ofnode_read_bool(node, "disable-memwipe");
	vboot->disable_lid_shutdown_during_update = ofnode_read_bool(node,
					"disable-lid-shutdown-during-update");
	vboot->disable_power_button_during_update = ofnode_read_bool(node,
					"disable-power-button-during-update");
#endif
	vboot->work_buffer_size = ofnode_read_u32_default(node,
					"vboot2-work-buffer-size", 0x3000);

	vboot->config = node;

	return 0;
}

void vboot_init_cparams(struct vboot_info *vboot, VbCommonParams *cparams)
{
#ifdef CONFIG_SYS_COREBOOT
	cparams->shared_data_blob =
		&((chromeos_acpi_t *)lib_sysinfo.vdat_addr)->vdat;
	cparams->shared_data_size =
		sizeof(((chromeos_acpi_t *)lib_sysinfo.vdat_addr)->vdat);
#else
	/*
	 * TODO(sjg@chromium.org): Implement this
	 *	cparams->shared_data_blob = vboot->vb_shared_data;
	 *	cparams->shared_data_size = VB_SHARED_DATA_REC_SIZE;
	 */
#endif
	log_debug("cparams:\n");
	log_debug("- %-20s: %08x\n", "shared_data_blob",
		  (uint)map_to_sysmem(cparams->shared_data_blob));
	log_debug("- %-20s: %08x\n", "shared_data_size",
		  cparams->shared_data_size);
}

bool vboot_is_slot_a(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return !(ctx->flags & VB2_CONTEXT_FW_SLOT_B);
}

const char *vboot_slot_name(struct vboot_info *vboot)
{
	return vboot_is_slot_a(vboot) ? "A" : "B";
}

void vboot_set_selected_region(struct vboot_info *vboot,
			       const struct fmap_entry *spl,
			       const struct fmap_entry *u_boot)
{
	vboot->blob->spl_entry = *spl;
	vboot->blob->u_boot_entry = *u_boot;
}

int vboot_platform_is_resuming(void)
{
	/* TODO(sjg@chromium.org): Implement this */

	return 0;
}

int vboot_dump_nvdata(const void *nvdata, int size)
{
	const u8 *data = nvdata;
	uint sig, val, crc, crc_ofs, ch;
	uint expect_size;
	bool is_v2;

	ch = data[VB2_NV_OFFS_HEADER];
	sig = ch & VB2_NV_HEADER_SIGNATURE_MASK;
	is_v2 = sig == VB2_NV_HEADER_SIGNATURE_V2;

	crc_ofs = is_v2 ? VB2_NV_OFFS_CRC_V2 : VB2_NV_OFFS_CRC_V1;
	crc = crc8(0, data, crc_ofs);
	printf("Vboot nvdata:\n");
	printf("   Signature %s\n", sig == VB2_NV_HEADER_SIGNATURE_V1 ? "v1" :
	       sig == VB2_NV_HEADER_SIGNATURE_V2 ? "v2" : "invalid");
	expect_size = sig == VB2_NV_HEADER_SIGNATURE_V1 ? VB2_NVDATA_SIZE :
		sig == VB2_NV_HEADER_SIGNATURE_V2 ? VB2_NVDATA_SIZE_V2 : -1;
	printf("   Size %d : %svalid\n", size, size == expect_size ? "" : "in");
	printf("   CRC %x (calc %x): %svalid\n", data[crc_ofs], crc,
	       crc == data[crc_ofs] ? "" : "in");

	if (ch & VB2_NV_HEADER_WIPEOUT)
		printf("   - wipeout\n");
	if (ch & VB2_NV_HEADER_KERNEL_SETTINGS_RESET)
		printf("   - kernel settings reset\n");
	if (ch & VB2_NV_HEADER_FW_SETTINGS_RESET)
		printf("   - firmware settings reset\n");

	ch = data[VB2_NV_OFFS_BOOT];
	printf("   Try count %d\n", ch & VB2_NV_BOOT_TRY_COUNT_MASK);
	if (ch & VB2_NV_BOOT_BACKUP_NVRAM)
		printf("   - backup nvram\n");
	if (ch & VB2_NV_BOOT_OPROM_NEEDED)
		printf("   - oprom needed\n");
	if (ch & VB2_NV_BOOT_DISABLE_DEV)
		printf("   - disable dev\n");
	if (ch & VB2_NV_BOOT_DEBUG_RESET)
		printf("   - debug reset\n");

	ch = data[VB2_NV_OFFS_BOOT2];
	printf("   Result %d\n", ch & VB2_NV_BOOT2_RESULT_MASK);
	if (ch & VB2_NV_BOOT2_TRIED)
		printf("   - tried\n");
	if (ch & VB2_NV_BOOT2_TRY_NEXT)
		printf("   - try next\n");
	printf("   Prev result %d\n", (ch & VB2_NV_BOOT2_PREV_RESULT_MASK) >>
	       VB2_NV_BOOT2_PREV_RESULT_SHIFT);
	if (ch & VB2_NV_BOOT2_PREV_TRIED)
		printf("   - prev tried\n");
	printf("   Recovery %d\n", data[VB2_NV_OFFS_RECOVERY]);
	printf("   Recovery subcode %d\n", data[VB2_NV_OFFS_RECOVERY_SUBCODE]);
	printf("   Localization %d\n", data[VB2_NV_OFFS_LOCALIZATION]);

	ch = data[VB2_NV_OFFS_DEV];
	if (ch & VB2_NV_DEV_FLAG_USB)
		printf("   - dev usb\n");
	if (ch & VB2_NV_DEV_FLAG_SIGNED_ONLY)
		printf("   - dev signed only\n");
	if (ch & VB2_NV_DEV_FLAG_LEGACY)
		printf("   - dev legacy\n");
	if (ch & VB2_NV_DEV_FLAG_FASTBOOT_FULL_CAP)
		printf("   - dev fastboot full cap\n");
	printf("   Default boot %d\n", (ch & VB2_NV_DEV_FLAG_DEFAULT_BOOT) >>
	       VB2_NV_DEV_DEFAULT_BOOT_SHIFT);
	if (ch & VB2_NV_DEV_FLAG_UDC)
		printf("   - dev udc\n");

	ch = data[VB2_NV_OFFS_TPM];
	if (ch & VB2_NV_TPM_CLEAR_OWNER_REQUEST)
		printf("   - TPM clear owner request needed\n");
	if (ch & VB2_NV_TPM_CLEAR_OWNER_DONE)
		printf("   - TPM clear owner done\n");
	if (ch & VB2_NV_TPM_REBOOTED)
		printf("   - TPM rebooted\n");

	ch = data[VB2_NV_OFFS_MISC];
	if (ch & VB2_NV_MISC_UNLOCK_FASTBOOT)
		printf("   - unlock fastboot\n");
	if (ch & VB2_NV_MISC_BOOT_ON_AC_DETECT)
		printf("   - boot-on-AC detect\n");
	if (ch & VB2_NV_MISC_TRY_RO_SYNC)
		printf("   - try RO sync\n");
	if (ch & VB2_NV_MISC_BATTERY_CUTOFF)
		printf("   - battery cutoff\n");
	if (ch & VB2_NV_MISC_ENABLE_ALT_OS)
		printf("   - enable Alt OS\n");
	if (ch & VB2_NV_MISC_DISABLE_ALT_OS)
		printf("   - eisable Alt OS\n");
	if (ch & VB2_NV_MISC_POST_EC_SYNC_DELAY)
		printf("   - post EC-sync delay\n");

	val = data[VB2_NV_OFFS_KERNEL1] | data[VB2_NV_OFFS_KERNEL2] << 8;
	printf("   Kernel %d\n", val);

	val = data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD1] |
		data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD2] << 8 |
		data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD3] << 16 |
		data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD4] << 24;
	printf("   Kernel max roll-forward %d\n", val);

	return crc ? -EINVAL : 0;
}

int vboot_dump_secdata(const void *secdata, int size)
{
	const struct vb2_secdata *sec = secdata;
	uint crc;

	crc = crc8(0, secdata, offsetof(struct vb2_secdata, crc8));
	printf("Vboot secdata:\n");

	print_buffer(0, secdata, 1, size, 0);
	printf("   Size %d : %svalid\n", size, size == VB2_SECDATA_SIZE ?
	       "" : "in");
	printf("   CRC %x (calc %x): %svalid\n", sec->crc8, crc,
	       crc == sec->crc8 ? "" : "in");
	printf("   Version %d\n", sec->struct_version);
	if (sec->flags & VB2_SECDATA_FLAG_LAST_BOOT_DEVELOPER)
		printf("   - last boot was dev mode\n");
	if (sec->flags & VB2_SECDATA_FLAG_DEV_MODE)
		printf("   - dev mode\n");
	printf("   Firmware versions %x\n", sec->fw_versions);


	return 0;
}
