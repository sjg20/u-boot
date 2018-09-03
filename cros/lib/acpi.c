// SPDX-License-Identifier: GPL-2.0
/*
 * Writing of vboot state into ACPI tables
 *
 * Copyright 2020 Google LLC
 */

#define LOG_CATEGORY	LOGC_VBOOT

#include <common.h>
#include <bloblist.h>
#include <asm/cb_sysinfo.h>
#include <log.h>
#include <smbios.h>
#include <asm/intel_gnvs.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>

#include <vboot_struct.h>

#include <vb2_internals_please_do_not_use.h>

/**
 * get_firmware_index() - Get the encoded firmware index
 *
 * @vbsd_fw_index: vboot firmware index
 * @return associated index for using in ACPI
 */
static int get_firmware_index(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_context();

	if (vboot_is_recovery(vboot))
		return BINF_RECOVERY;
	else
		return vboot_is_slot_a(vboot) ? BINF_RW_A : BINF_RW_B;
}

int vboot_update_acpi(struct vboot_info *vboot, enum cros_fw_type_t fw_type)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	struct chromeos_acpi_gnvs *tab;
	struct acpi_global_nvs *gnvs;
	struct vb2_gbb_header *gbb;
	VbSharedDataHeader *vb_sd;
	VbSharedDataHeader *vdat;
	uint size, vb_sd_size;
	ulong fmap_addr;
	int main_fw;
	char *hwid;
	uint chsw;
	int ret;

	log_info("Updating ACPI tables\n");
	gnvs = bloblist_find(BLOBLISTT_ACPI_GNVS, sizeof(*gnvs));
	if (!gnvs) {
		if (!vboot_from_cb(vboot))
			return log_msg_ret("bloblist", -ENOENT);
		gnvs = vboot->sysinfo->acpi_gnvs;
		if (!gnvs)
			return log_msg_ret("gnvs", -ENOKEY);
	}
	tab = &gnvs->chromeos;

	/* Write VbSharedDataHeader to ACPI vdat for userspace access. */
	vb2api_export_vbsd(ctx, tab->vdat);

	acpi_table->boot_reason = BOOT_REASON_OTHER;

	tab->main_fw_type = get_firmware_index(ctx);

	if (vboot->ec_software_sync) {
		int in_rw = 0;

		if (vb2ex_ec_running_rw(&in_rw)) {
			log_err("Couldn't tell if the EC firmware is RW\n");
			return -EPROTO;
		}
		tab->activeec_fw = in_rw ? ACTIVE_ECFW_RW : ACTIVE_ECFW_RO;
	}

	chsw = 0;
	if (ctx->flags & VB2_CONTEXT_FORCE_RECOVERY_MODE)
		chsw |= CHSW_RECOVERY_X86;
	if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
		chsw |= CHSW_DEVELOPER_SWITCH;
	tab->switches = chsw;

	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = MIN(sizeof(hwid), sizeof(acpi_table->hwid));
	if (!vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size))
		memcpy(tab->hwid, hwid, hwid_size);

	size = min(ID_LEN, sizeof(tab->fwid));
	memcpy(tab->fwid, vboot->firmware_id, size);

	size = min(ID_LEN, sizeof(tab->frid));
	memcpy(tab->frid, vboot->readonly_firmware_id, size);

	if (fw_type != FIRMWARE_TYPE_AUTO_DETECT)
		tab->main_fw_type = firmware_type;
	else if (main_fw == BINF_RECOVERY)
		tab->main_fw_type = FIRMWARE_TYPE_RECOVERY;
	else if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
		tab->main_fw_type = FIRMWARE_TYPE_DEVELOPER;
	else
		tab->main_fw_type = FIRMWARE_TYPE_NORMAL;

	tab->recovery_reason = vb2api_get_recovery_reason(ctx);

	if (!vboot->fmap.readonly.fmap.length) {
		log_err("No FMAP available\n");
		return -ENOTDIR;
	}
	ret = fwstore_entry_mmap(vboot->fwstore, &vboot->fmap.readonly.fmap,
				 &fmap_addr);
	if (!ret)
		tab->fmap_base = fmap_addr;
	else
		log_warning("FMAP address cannot be mapped (err=%d)\n", ret);

	if (IS_ENABLED(CONFIG_GENERATE_SMBIOS_TABLE)) {
		size = min(ID_LEN, sizeof(tab->fwid));
		ret = smbios_update_version(vboot->firmware_id);
		if (ret) {
			log_err("Unable to update SMBIOS type 0 version string\n");
			return log_msg_ret("smbios", ret);
		}
	} else if (vboot_from_cb(vboot)) {
		void *smbios_start;

		smbios_start = (void *)(ulong)vboot->sysinfo->smbios_start;
		if (!smbios_start) {
			log_warning("SMBIOS table not provided\n");
			return log_msg_ret("tab", -ENOENT);
		}
		ret = smbios_update_version_full(smbios_start,
						 vboot->firmware_id);
		if (ret) {
			log_err("Unable to update SMBIOS type 0 version string\n");
			return log_msg_ret("cbsmbios", ret);
		}
	}

	return 0;
}

