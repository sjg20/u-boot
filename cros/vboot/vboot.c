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

static struct nvdata_info {
	u8 ofs;
	u8 mask;
	const char *name;
} nvdata_info[] = {
	{ VB2_NV_OFFS_HEADER, VB2_NV_HEADER_WIPEOUT, "wipeout" },
	{ VB2_NV_OFFS_HEADER, VB2_NV_HEADER_KERNEL_SETTINGS_RESET,
		"kernel settings reset" },
	{ VB2_NV_OFFS_HEADER, VB2_NV_HEADER_FW_SETTINGS_RESET,
		"firmware settings reset" },
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_BACKUP_NVRAM, "backup nvram" },
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_OPROM_NEEDED, "oprom needed" },
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_DISABLE_DEV, "disable dev" },
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_DEBUG_RESET, "debug reset" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_TRIED, "tried" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_TRY_NEXT, "try next" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_PREV_TRIED, "prev tried" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_USB, "dev usb" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_SIGNED_ONLY, "dev signed only" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_LEGACY, "dev legacy" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_FASTBOOT_FULL_CAP,
		"dev fastboot full cap" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_UDC, "dev udc" },
	{ VB2_NV_OFFS_TPM, VB2_NV_TPM_CLEAR_OWNER_REQUEST,
		"TPM clear owner request needed" },
	{ VB2_NV_OFFS_TPM, VB2_NV_TPM_CLEAR_OWNER_DONE, "TPM clear owner done" },
	{ VB2_NV_OFFS_TPM, VB2_NV_TPM_REBOOTED, "TPM rebooted" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_UNLOCK_FASTBOOT, "unlock fastboot" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_BOOT_ON_AC_DETECT,
		"boot-on-AC detect" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_TRY_RO_SYNC, "try RO sync" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_BATTERY_CUTOFF, "battery cutoff" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_ENABLE_ALT_OS, "enable Alt OS" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_DISABLE_ALT_OS, "disable Alt OS" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_POST_EC_SYNC_DELAY,
		"post EC-sync delay" },
	{}
};

int vboot_dump_nvdata(const void *nvdata, int size)
{
	const u8 *data = nvdata;
	uint sig, val, val2, crc, crc_ofs, ch;
	static struct nvdata_info *inf;
	uint expect_size;
	bool is_v2;

	ch = data[VB2_NV_OFFS_HEADER];
	sig = ch & VB2_NV_HEADER_SIGNATURE_MASK;
	is_v2 = sig == VB2_NV_HEADER_SIGNATURE_V2;

	crc_ofs = is_v2 ? VB2_NV_OFFS_CRC_V2 : VB2_NV_OFFS_CRC_V1;
	crc = crc8(0, data, crc_ofs);
	printf("Vboot nvdata:\n");
	printf("   Signature %s, ", sig == VB2_NV_HEADER_SIGNATURE_V1 ? "v1" :
	       sig == VB2_NV_HEADER_SIGNATURE_V2 ? "v2" : "invalid");
	expect_size = sig == VB2_NV_HEADER_SIGNATURE_V1 ? VB2_NVDATA_SIZE :
		sig == VB2_NV_HEADER_SIGNATURE_V2 ? VB2_NVDATA_SIZE_V2 : -1;
	printf("size %d (%svalid), ", size, size == expect_size ? "" : "in");
	printf("CRC %x (calc %x, %svalid)\n", data[crc_ofs], crc,
	       crc == data[crc_ofs] ? "" : "in");

	for (inf = nvdata_info; inf->name; inf++) {
		if (data[inf->ofs] & inf->mask)
			printf("   - %s\n", inf->name);
	}

	ch = data[VB2_NV_OFFS_BOOT2];
	printf("   Result %d, prev %d\n", ch & VB2_NV_BOOT2_RESULT_MASK,
	       (ch & VB2_NV_BOOT2_PREV_RESULT_MASK) >>
	       VB2_NV_BOOT2_PREV_RESULT_SHIFT);
	printf("   Recovery %x, subcode %x\n", data[VB2_NV_OFFS_RECOVERY],
	       data[VB2_NV_OFFS_RECOVERY_SUBCODE]);

	ch = data[VB2_NV_OFFS_DEV];
	val = data[VB2_NV_OFFS_KERNEL1] | data[VB2_NV_OFFS_KERNEL2] << 8;
	val2 = data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD1] |
		data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD2] << 8 |
		data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD3] << 16 |
		data[VB2_NV_OFFS_KERNEL_MAX_ROLLFORWARD4] << 24;
	printf("   Localization %d, default boot %d, kernel %x, max roll-forward %x\n",
	       data[VB2_NV_OFFS_LOCALIZATION],
	       (ch & VB2_NV_DEV_FLAG_DEFAULT_BOOT) >>
	       VB2_NV_DEV_DEFAULT_BOOT_SHIFT, val, val2);

	return crc ? -EINVAL : 0;
}

int vboot_dump_secdata(const void *secdata, int size)
{
	const struct vb2_secdata *sec = secdata;
	uint crc;

	crc = crc8(0, secdata, offsetof(struct vb2_secdata, crc8));
	printf("Vboot secdata:\n");

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
