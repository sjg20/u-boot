/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _SPL_RELOC_H
#define _SPL_RELOC_H

#include <mapmem.h>
#include <spl.h>
#include <asm/global_data.h>
#include <linux/types.h>

DECLARE_GLOBAL_DATA_PTR;

static ulong spl_check_layout(ulong *sizep)
{
	ulong fdt_end, fdt_size;
	ulong sp;

	sp = map_to_sysmem(&sp);
	fdt_size = fdt_totalsize(gd->fdt_blob);
	fdt_end = map_to_sysmem(gd->fdt_blob) + fdt_size;
	log_info("stack %lx fdt_size %lx fdt_end %lx space %lx\n", sp, fdt_size,
		 fdt_end, sp - fdt_end);
	*sizep = sp - fdt_end;

	return ALIGN(fdt_end, 4);
}

int spl_reloc_prepare(struct spl_image_info *image, ulong *addrp)
{
	ulong buf_addr, buf_size;

	buf_addr = spl_check_layout(&buf_size);
	if (buf_size < image->size) {
		log_err("Image size %x but buffer is only %lx\n", image->size,
			buf_size);
		return -ENOSPC;
	}

	return 0;
}

void spl_reloc_jump(struct spl_image_info *image, spl_jump_to_image_t func)
{
}

#endif /* _SPL_RELOC_H */
