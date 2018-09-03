// SPDX-License-Identifier: GPL-2.0+
/*
 * Access to internal vboot data for debugging / development
 *
 * Copyright 2020 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT
#define NEED_VB20_INTERNALS

#include <common.h>
#include <cros/vboot.h>
#include <u-boot/crc.h>

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
	bool is_v2, crc_ok;
	uint expect_size;

	ch = data[VB2_NV_OFFS_HEADER];
	sig = ch & VB2_NV_HEADER_SIGNATURE_MASK;
	is_v2 = sig == VB2_NV_HEADER_SIGNATURE_V2;

	crc_ofs = is_v2 ? VB2_NV_OFFS_CRC_V2 : VB2_NV_OFFS_CRC_V1;
	crc = crc8(0, data, crc_ofs);
	crc_ok = crc == data[crc_ofs];
	printf("Vboot nvdata:\n");
	printf("   Signature %s, ", sig == VB2_NV_HEADER_SIGNATURE_V1 ? "v1" :
	       sig == VB2_NV_HEADER_SIGNATURE_V2 ? "v2" : "invalid");
	expect_size = sig == VB2_NV_HEADER_SIGNATURE_V1 ? VB2_NVDATA_SIZE :
		sig == VB2_NV_HEADER_SIGNATURE_V2 ? VB2_NVDATA_SIZE_V2 : -1;
	printf("size %d (%svalid), ", size, size == expect_size ? "" : "in");
	printf("CRC %x (calc %x, %svalid)\n", data[crc_ofs], crc,
	       crc_ok ? "" : "in");

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

	return crc_ok ? 0 : -EINVAL;
}

int vboot_secdata_dump(const void *secdata, int size)
{
	const struct vb2_secdata *sec = secdata;
	bool crc_ok;
	uint crc;

	crc = crc8(0, secdata, offsetof(struct vb2_secdata, crc8));
	crc_ok = crc == sec->crc8;
	printf("Vboot secdata:\n");
	print_buffer(0, secdata, 1, size, 0);

	printf("   Size %d : %svalid\n", size, size == VB2_SECDATA_SIZE ?
	       "" : "in");
	printf("   CRC %x (calc %x): %svalid\n", sec->crc8, crc,
	       crc_ok ? "" : "in");
	printf("   Version %d\n", sec->struct_version);
	if (sec->flags & VB2_SECDATA_FLAG_LAST_BOOT_DEVELOPER)
		printf("   - last boot was dev mode\n");
	if (sec->flags & VB2_SECDATA_FLAG_DEV_MODE)
		printf("   - dev mode\n");
	printf("   Firmware versions %x\n", sec->fw_versions);

	return crc_ok ? 0 : -EINVAL;
}

static void update_flag(u8 *flagp, uint mask, uint val)
{
	if (val)
		*flagp |= mask;
	else
		*flagp &= ~mask;
}

int vboot_secdata_set(void *secdata, int size, enum secdata_t field, int val)
{
	struct vb2_secdata *sec = secdata;

	switch (field) {
	case SECDATA_LAST_BOOT_DEV:
		update_flag(&sec->flags, VB2_SECDATA_FLAG_LAST_BOOT_DEVELOPER,
			    val);
		break;
	case SECDATA_DEV_MODE:
		update_flag(&sec->flags, VB2_SECDATA_FLAG_DEV_MODE, val);
		break;
	default:
		return -ENOENT;
	}

	/* Update the CRC */
	sec->crc8 = crc8(0, secdata, offsetof(struct vb2_secdata, crc8));

	return 0;
}

int vboot_secdata_get(const void *secdata, int size, enum secdata_t field)
{
	const struct vb2_secdata *sec = secdata;

	switch (field) {
	case SECDATA_LAST_BOOT_DEV:
		return sec->flags & VB2_SECDATA_FLAG_LAST_BOOT_DEVELOPER ?
			true : false;
	case SECDATA_DEV_MODE:
		return sec->flags & VB2_SECDATA_FLAG_DEV_MODE ? true : false;
	default:
		return -ENOENT;
	}
}
