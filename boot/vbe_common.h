/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Verified Boot for Embedded (VBE) common functions
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __VBE_COMMON_H
#define __VBE_COMMON_H

#include <linux/types.h>

#define USE_BOOTMETH	false

struct spl_image_info;
struct spl_load_info;

enum {
	MAX_VERSION_LEN		= 256,

	NVD_HDR_VER_SHIFT	= 0,
	NVD_HDR_VER_MASK	= 0xf,
	NVD_HDR_SIZE_SHIFT	= 4,
	NVD_HDR_SIZE_MASK	= 0xf << NVD_HDR_SIZE_SHIFT,

	/* Firmware key-version is in the top 16 bits of fw_ver */
	FWVER_KEY_SHIFT		= 16,
	FWVER_FW_MASK		= 0xffff,

	NVD_HDR_VER_CUR		= 1,	/* current version */
};

/**
 * enum vbe_try_result - result of trying a firmware pick
 *
 * @VBETR_UNKNOWN: Unknown / invalid result
 * @VBETR_TRYING: Firmware pick is being tried
 * @VBETR_OK: Firmware pick is OK and can be used from now on
 * @VBETR_BAD: Firmware pick is bad and should be removed
 */
enum vbe_try_result {
	VBETR_UNKNOWN,
	VBETR_TRYING,
	VBETR_OK,
	VBETR_BAD,
};

/**
 * enum vbe_flags - flags controlling operation
 *
 * @VBEF_TRY_COUNT_MASK: mask for the 'try count' value
 * @VBEF_TRY_B: Try the B slot
 * @VBEF_RECOVERY: Use recovery slot
 */
enum vbe_flags {
	VBEF_TRY_COUNT_MASK	= 0x3,
	VBEF_TRY_B	= BIT(2),
	VBEF_RECOVERY	= BIT(3),

	VBEF_RESULT_SHIFT	= 4,
	VBEF_RESULT_MASK	= 3 << VBEF_RESULT_SHIFT,

	VBEF_PICK_SHIFT		= 6,
	VBEF_PICK_MASK		= 3 << VBEF_PICK_SHIFT,
};

/**
 * struct vbe_nvdata - basic storage format for non-volatile data
 *
 * This is used for all VBE methods
 *
 * @crc8: crc8 for the entire record except @crc8 field itself
 * @hdr: header size and version (NVD_HDR_...)
 * @spare1: unused, must be 0
 * @fw_vernum: version and key version (FWVER_...)
 * @flags: Flags controlling operation (enum vbe_flags)
 */
struct vbe_nvdata {
	u8 crc8;
	u8 hdr;
	u16 spare1;
	u32 fw_vernum;
	u32 flags;
	u8 spare2[0x34];
};

ulong h_vbe_load_read(struct spl_load_info *load, ulong off, ulong size,
		      void *buf);

int vbe_read_fit(struct udevice *blk, ulong area_offset, ulong area_size,
		 struct spl_image_info *image, ulong *load_addrp, char **namep);

ofnode vbe_get_node(void);

int vbe_read_nvdata(struct udevice *blk, ulong offset, ulong size, u8 *buf);

int vbe_read_version(struct udevice *blk, ulong offset, char *version,
		     int max_size);

int vbe_get_blk(const char *storage, struct udevice **blkp);

#endif /* __VBE_ABREC_H */
