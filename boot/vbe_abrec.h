/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Verified Boot for Embedded (VBE) vbe-abrec common file
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __VBE_ABREC_H
#define __VBE_ABREC_H

struct bootflow;
struct udevice;

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

/** struct abrec_priv - information read from the device tree */
struct abrec_priv {
	u32 area_start;
	u32 area_size;
	u32 skip_offset;
	u32 state_offset;
	u32 state_size;
	u32 version_offset;
	u32 version_size;
	const char *storage;
};

/**
 * enum vbe_pick_t- indicates which firmware is picked
 *
 * @VBEFT_A: Firmware A
 * @VBEFT_B: Firmware B
 * @VBEFT_RECOVERY: Recovery firmware
 */
enum vbe_pick_t {
	VBEP_A,
	VBEP_B,
	VBEP_RECOVERY,
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
	VBEF_RESULT_UNKNOWN	= 0,
	VBEF_RESULT_TRYING	= 1,
	VBEF_RESULT_OK		= 2,
	VBEF_RESULT_BAD		= 3,
};

/** struct abrec_state - state information read from media
 *
 * @fw_version: Firmware version string
 * @fw_vernum: Firmware version number
 * @flags: Flags controlling operation (enum vbe_flags)
 */
struct abrec_state {
	char fw_version[MAX_VERSION_LEN];
	u32 fw_vernum;
	u32 flags;
};

/**
 * abrec_read_fw_bootflow() - Read a bootflow for firmware
 *
 * Locates and loads the firmware image (FIT) needed for the next phase. The FIT
 * should ideally use external data, to reduce the amount of it that needs to be
 * read.
 *
 * @bdev: bootdev device containing the firmwre
 * @blow: Place to put the created bootflow, on success
 * @return 0 if OK, -ve on error
 */
int abrec_read_bootflow_fw(struct udevice *dev, struct bootflow *bflow);

int abrec_read_state(struct abrec_priv *priv, struct udevice *blk, void *buf,
		     struct abrec_state *state);

int abrec_read_priv(ofnode node, struct abrec_priv *priv);

#endif /* __VBE_ABREC_H */
