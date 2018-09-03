// SPDX-License-Identifier: GPL-2.0+
/*
 * Access to internal vboot data for debugging / development
 *
 * Copyright 2020 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <cros/vboot.h>
#include <u-boot/crc.h>

#include <vb2_internals_please_do_not_use.h>

/* Used for secdatak */
#define MAJOR_VER(x) (((x) & 0xf0) >> 4)
#define MINOR_VER(x) ((x) & 0x0f)

static const struct nvdata_info {
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
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_DISPLAY_REQUEST, "oprom needed" },
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_DISABLE_DEV, "disable dev" },
	{ VB2_NV_OFFS_BOOT, VB2_NV_BOOT_DEBUG_RESET, "debug reset" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_TRIED, "tried" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_TRY_NEXT, "try next" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_PREV_TRIED, "prev tried" },
	{ VB2_NV_OFFS_BOOT2, VB2_NV_BOOT2_REQ_DIAG, "diag req" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_EXTERNAL, "dev external" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_SIGNED_ONLY, "dev signed only" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_LEGACY, "dev legacy" },
	{ VB2_NV_OFFS_DEV, VB2_NV_DEV_FLAG_UDC, "dev udc" },
	{ VB2_NV_OFFS_TPM, VB2_NV_TPM_CLEAR_OWNER_REQUEST,
		"TPM clear owner request needed" },
	{ VB2_NV_OFFS_TPM, VB2_NV_TPM_CLEAR_OWNER_DONE, "TPM clear owner done" },
	{ VB2_NV_OFFS_TPM, VB2_NV_TPM_REBOOTED, "TPM rebooted" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_BOOT_ON_AC_DETECT,
		"boot-on-AC detect" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_TRY_RO_SYNC, "try RO sync" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_BATTERY_CUTOFF, "battery cutoff" },
	{ VB2_NV_OFFS_MISC, VB2_NV_MISC_POST_EC_SYNC_DELAY,
		"post EC-sync delay" },
	{}
};

int vboot_nvdata_dump(const void *nvdata, int size)
{
	const u8 *data = nvdata;
	uint sig, val, val2, crc, crc_ofs, ch;
	const struct nvdata_info *inf;
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

int vboot_secdataf_dump(const void *data, int size)
{
	const struct vb2_secdata_firmware *sec = data;
	bool crc_ok;
	uint crc;

	crc = crc8(0, data, offsetof(struct vb2_secdata_firmware, crc8));
	crc_ok = crc == sec->crc8;
	printf("Vboot secdataf:\n");
	print_buffer(0, data, 1, size, 0);

	printf("   Size %d : %svalid\n", size,
	       size == VB2_SECDATA_FIRMWARE_SIZE ? "" : "in");
	printf("   CRC %x (calc %x): %svalid\n", sec->crc8, crc,
	       crc_ok ? "" : "in");
	printf("   Version %d\n", sec->struct_version);
	if (sec->flags & VB2_SECDATA_FIRMWARE_FLAG_LAST_BOOT_DEVELOPER)
		printf("   - last boot was dev mode\n");
	if (sec->flags & VB2_SECDATA_FIRMWARE_FLAG_DEV_MODE)
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

int vboot_secdataf_set(void *data, int size, enum secdata_t field, int val)
{
	struct vb2_secdata_firmware *sec = data;

	switch (field) {
	case SECDATA_LAST_BOOT_DEV:
		update_flag(&sec->flags,
			    VB2_SECDATA_FIRMWARE_FLAG_LAST_BOOT_DEVELOPER, val);
		break;
	case SECDATA_DEV_MODE:
		update_flag(&sec->flags, VB2_SECDATA_FIRMWARE_FLAG_DEV_MODE,
			    val);
		break;
	default:
		return -ENOENT;
	}

	/* Update the CRC */
	sec->crc8 = crc8(0, data, offsetof(struct vb2_secdata_firmware, crc8));

	return 0;
}

int vboot_secdataf_get(const void *data, int size, enum secdata_t field)
{
	const struct vb2_secdata_firmware *sec = data;

	switch (field) {
	case SECDATA_LAST_BOOT_DEV:
		return sec->flags & VB2_SECDATA_FIRMWARE_FLAG_LAST_BOOT_DEVELOPER ?
			true : false;
	case SECDATA_DEV_MODE:
		return sec->flags & VB2_SECDATA_FIRMWARE_FLAG_DEV_MODE ? true :
			 false;
	default:
		return -ENOENT;
	}
}

static const struct secdatak_info {
	u8 mask;
	const char *name;
} secdatak_info[] = {
	{ VB2_SECDATA_KERNEL_FLAG_PHONE_RECOVERY_DISABLED,
		"phone-rec-disable" },
	{ VB2_SECDATA_KERNEL_FLAG_PHONE_RECOVERY_UI_DISABLED,
		"phone-rec-ui-disable" },
	{ VB2_SECDATA_KERNEL_FLAG_DIAGNOSTIC_UI_DISABLED,
		"diag-ui-disabled" },
	{ VB2_SECDATA_KERNEL_FLAG_HWCRYPTO_ALLOWED,
		"hw-crypto-allowed" },
	{}
};

int vboot_secdatak_dump(const void *data, int size)
{
	const struct vb2_secdata_kernel_v0 *v0 = data;
	const struct vb2_secdata_kernel_v1 *v1 = data;
	bool is_v0, is_v1, valid;
	int cofs, csize, cexp;
	uint major, minor;
	bool crc_ok;
	uint crc;

	printf("Vboot secdatak:\n");
	major = MAJOR_VER(v0->struct_version);
	minor = MINOR_VER(v0->struct_version);
	is_v1 = major == MAJOR_VER(VB2_SECDATA_KERNEL_VERSION_V10);
	is_v0 = !major && minor == MINOR_VER(VB2_SECDATA_KERNEL_VERSION_V02);
	valid = is_v0 || is_v1;
	print_buffer(0, data, 1, size, 0);
	printf("   Version %02x (major %x, minor %x) - %svalid\n",
	       v0->struct_version, major, minor, valid ? "" : "in");
	if (is_v0) {
		cofs = 0;
		csize = offsetof(struct vb2_secdata_kernel_v0, crc8);
		cexp = v0->crc8;
	} else {
		cofs = offsetof(struct vb2_secdata_kernel_v1, flags);
		csize = v1->struct_size - cofs;
		cexp = v1->crc8;
	}
	crc = crc8(0, data + cofs, csize);
	crc_ok = crc == cexp;
	printf("   CRC %x (calc %x): %svalid\n", cexp, crc,
	       crc_ok ? "" : "in");
	if (is_v0) {
		printf("   UID %08x, versions %x\n", v0->uid,
		       v0->kernel_versions);
	} else {
		const struct secdatak_info *inf;
		int i;

		printf("   size %x, versions %x\n", v1->struct_size,
		       v1->kernel_versions);
		for (inf = secdatak_info; inf->name; inf++) {
			if (v1->flags & inf->mask)
				printf("   - %s\n", inf->name);
		}
		printf("   EC hash ");
		for (i = 0; i < VB2_SHA256_DIGEST_SIZE; i++)
			printf("%02x", v1->ec_hash[i]);
		printf("\n");
	}

	return valid && crc_ok ? 0 : -EINVAL;
}

static const struct fwmp_info {
	u8 mask;
	const char *name;
} fwmp_info[] = {
	{ VB2_SECDATA_FWMP_DEV_DISABLE_BOOT, "dev-boot-disable" },
	{ VB2_SECDATA_FWMP_DEV_DISABLE_RECOVERY, "dev-rec-disable" },
	{ VB2_SECDATA_FWMP_DEV_ENABLE_EXTERNAL, "dev-external-enable" },
	{ VB2_SECDATA_FWMP_DEV_ENABLE_ALTFW, "dev-altfw-enable" },
	{ VB2_SECDATA_FWMP_DEV_ENABLE_OFFICIAL_ONLY, "official-only" },
	{ VB2_SECDATA_FWMP_DEV_USE_KEY_HASH, "use-key-hash" },
	{ VB2_SECDATA_FWMP_DEV_DISABLE_CCD_UNLOCK, "ccd-unlock-disable" },
	{ VB2_SECDATA_FWMP_DEV_FIPS_MODE, "fips-mode" },
	{}
};

int vboot_fwmp_dump(const void *data, int size)
{
	const struct vb2_secdata_fwmp *fwmp = data;
	const struct fwmp_info *inf;
	uint major, minor;
	bool valid, size_ok;
	bool crc_ok;
	uint crc;
	int cofs;
	int i;

	printf("Vboot fwmp:\n");
	major = MAJOR_VER(fwmp->struct_version);
	minor = MINOR_VER(fwmp->struct_version);
	valid = major == MAJOR_VER(VB2_SECDATA_FWMP_VERSION);
	print_buffer(0, data, 1, size, 0);
	printf("   Version %02x (major %x, minor %x) - %svalid\n",
	       fwmp->struct_version, major, minor, valid ? "" : "in");
	printf("   Size %x: ", fwmp->struct_size);
	if (fwmp->struct_size < VB2_SECDATA_FWMP_MIN_SIZE)
		printf("too small");
	else if (fwmp->struct_size > size)
		printf("missing %x bytes", fwmp->struct_size - size);
	else if (fwmp->struct_size > VB2_SECDATA_FWMP_MAX_SIZE)
		printf("too large");
	else
		size_ok = true;
	printf("%s\n", size_ok ? "OK" : "");

	cofs = offsetof(struct vb2_secdata_fwmp, struct_version);
	crc = crc8(0, data + cofs, fwmp->struct_size - cofs);
	crc_ok = crc == fwmp->crc8;
	printf("   CRC %x (calc %x): %svalid\n", fwmp->crc8, crc,
	       crc_ok ? "" : "in");

	for (inf = fwmp_info; inf->name; inf++) {
		if (fwmp->flags & inf->mask)
			printf("   - %s\n", inf->name);
	}

	printf("   Dev kernel key ");
	for (i = 0; i < VB2_SECDATA_FWMP_HASH_SIZE; i++)
		printf("%02x", fwmp->dev_key_hash[i]);
	printf("\n");

	return valid && crc_ok ? 0 : -EINVAL;
}
