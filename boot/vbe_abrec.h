/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Verified Boot for Embedded (VBE) vbe-abrec common file
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __VBE_ABREC_H
#define __VBE_ABREC_H

#include <vbe.h>
#include "vbe_common.h"

struct bootflow;
struct udevice;

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

/** struct abrec_state - state information read from media
 *
 * @fw_version: Firmware version string
 * @fw_vernum: Firmware version number
 */
struct abrec_state {
	char fw_version[MAX_VERSION_LEN];
	u32 fw_vernum;
	uint try_count;
	bool try_b;
	bool recovery;
	enum vbe_try_result try_result;
	enum vbe_pick_t pick;
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

int abrec_read_state(struct udevice *dev, struct abrec_state *state);

int abrec_read_nvdata(struct abrec_priv *priv, struct udevice *blk,
		      struct abrec_state *state);

int abrec_read_priv(ofnode node, struct abrec_priv *priv);

#endif /* __VBE_ABREC_H */
