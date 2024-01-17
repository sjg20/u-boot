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
#include <dm/ofnode.h>
#include "vbe_abrec.h"

int abrec_read_priv(ofnode node, struct abrec_priv *priv)
{
	memset(priv, '\0', sizeof(*priv));
	if (ofnode_read_u32(node, "area-start", &priv->area_start) ||
	    ofnode_read_u32(node, "area-size", &priv->area_size) ||
	    ofnode_read_u32(node, "version-offset", &priv->version_offset) ||
	    ofnode_read_u32(node, "version-size", &priv->version_size) ||
	    ofnode_read_u32(node, "state-offset", &priv->state_offset) ||
	    ofnode_read_u32(node, "state-size", &priv->state_size))
		return log_msg_ret("read", -EINVAL);
	ofnode_read_u32(node, "skip-offset", &priv->skip_offset);
	priv->storage = strdup(ofnode_read_string(node, "storage"));
	if (!priv->storage)
		return log_msg_ret("str", -EINVAL);

	return 0;
}

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
	memcpy(state, buf, sizeof(struct abrec_state));
	strlcpy(state->fw_version, buf, MAX_VERSION_LEN);
	log_debug("version=%s flags=%x\n", state->fw_version, state->flags);

	return 0;
}
