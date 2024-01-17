// SPDX-License-Identifier: GPL-2.0
/*
 * Verified Boot for Embedded (VBE) common functions
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <spl.h>
#include <linux/types.h>
#include "vbe_common.h"

ulong h_vbe_load_read(struct spl_load_info *load, ulong off, ulong size,
		      void *buf)
{
	struct blk_desc *desc = load->priv;
	lbaint_t sector = off >> desc->log2blksz;
	lbaint_t count = size >> desc->log2blksz;
	int ret;

	log_debug("vbe read log2blksz %x offset %lx sector %lx count %lx\n",
		  desc->log2blksz, (ulong)off, (long)sector, (ulong)count);
// 	print_buffer(0x000631f0, (void *)0x000631f0, 1, 0x200, 0);

	ret = blk_dread(desc, sector, count, buf) << desc->log2blksz;
	log_debug("ret=%d\n", ret);
	return ret;
}
