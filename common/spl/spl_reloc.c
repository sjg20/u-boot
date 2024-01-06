/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _SPL_RELOC_H
#define _SPL_RELOC_H

#define LOG_DEBUG

#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <asm/global_data.h>
#include <linux/types.h>

DECLARE_GLOBAL_DATA_PTR;

static uint rcode_size = 0xe00;

enum {
	/* margin to allow for stack growth */
	RELOC_STACK_MARGIN	= 0x800,

	/* align base address for DMA controllers which require it */
	BASE_ALIGN		= 0x200,
};

static int setup_layout(struct spl_image_info *image, ulong *addrp)
{
	ulong base, fdt_size;
	ulong limit;
	int buf_size, margin;

	limit = map_to_sysmem(&limit) - RELOC_STACK_MARGIN;
	fdt_size = fdt_totalsize(gd->fdt_blob);
	base = ALIGN(map_to_sysmem(gd->fdt_blob) + fdt_size + BASE_ALIGN - 1,
		     BASE_ALIGN);
	buf_size = limit - rcode_size - base;
	margin = buf_size - image->size;
	log_debug("limit %lx fdt_size %lx rcode_size %x base %lx avail %x need %x, margin%s%lx\n",
		  limit, fdt_size, rcode_size, base, buf_size, image->size,
		  margin >= 0 ? " " : " -", abs(margin));
	if (margin < 0) {
		log_err("Image size %x but buffer is only %x\n", image->size,
			buf_size);
		return -ENOSPC;
	}

	image->buf = map_sysmem(base, image->size);
	*addrp = base;

	return 0;
}

int spl_reloc_prepare(struct spl_image_info *image, ulong *addrp)
{
	int ret;

	ret = setup_layout(image, addrp);
	if (ret)
		return ret;

	return 0;
}

void spl_reloc_jump(struct spl_image_info *image, spl_jump_to_image_t func)
{
	uint *src, *end, *dst;

	log_debug("Copying image size %x from %lx to %lx\n",
		  (ulong)map_to_sysmem(image->buf), image->size,
		  image->load_addr);
	for (dst = map_sysmem(image->load_addr, image->size),
	     src = image->buf, end = src + ALIGN(image->size, sizeof(uint));
	     src < end;)
	     *dst++ = *src++;
}

#endif /* _SPL_RELOC_H */
