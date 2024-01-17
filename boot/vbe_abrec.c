// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified Boot for Embedded (VBE) 'simple' method
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY LOGC_BOOT

#include <common.h>
#include <mmc.h>
#include "vbe_abrec.h"

int abrec_read_state(struct abrec_priv *priv, struct udevice *blk, void *buf,
		     struct abrec_state *state)
{
	int start;

	if (priv->version_size > MMC_MAX_BLOCK_LEN)
		return log_msg_ret("ver", -E2BIG);

	start = priv->area_start + priv->version_offset;
	if (start & (MMC_MAX_BLOCK_LEN - 1))
		return log_msg_ret("get", -EBADF);
	start /= MMC_MAX_BLOCK_LEN;

	if (blk_read(blk, start, 1, buf) != 1)
		return log_msg_ret("read", -EIO);
	strlcpy(state->fw_version, buf, MAX_VERSION_LEN);
	log_debug("version=%s\n", state->fw_version);

	return 0;
}
