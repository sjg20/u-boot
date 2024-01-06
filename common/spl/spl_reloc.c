/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _SPL_RELOC_H
#define _SPL_RELOC_H

#define LOG_DEBUG

#include <display_options.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <linux/types.h>

DECLARE_GLOBAL_DATA_PTR;

enum {
	/* margin to allow for stack growth */
	RELOC_STACK_MARGIN	= 0x800,

	/* align base address for DMA controllers which require it */
	BASE_ALIGN		= 0x200,

	STACK_PROT_VALUE	= 0x51ce4697,
};

typedef void (*rcode_func)(struct spl_image_info *image,
			   spl_jump_to_image_t func);

static int setup_layout(struct spl_image_info *image, ulong *addrp)
{
	ulong base, fdt_size;
	ulong limit, rcode_base;
	uint rcode_size;
	int buf_size, margin;
	char *rcode_buf;

	limit = ALIGN(map_to_sysmem(&limit) - RELOC_STACK_MARGIN, 8);
	image->stack_prot = map_sysmem(limit, sizeof(uint));
	*image->stack_prot = STACK_PROT_VALUE;

	fdt_size = fdt_totalsize(gd->fdt_blob);
	base = ALIGN(map_to_sysmem(gd->fdt_blob) + fdt_size + BASE_ALIGN - 1,
		     BASE_ALIGN);

	rcode_size = _rcode_end - _rcode_start;
	rcode_base = limit - rcode_size;
	buf_size = rcode_base - base;
	margin = buf_size - image->size;
	log_debug("limit %lx fdt_size %lx base %lx avail %x need %x, margin%s%lx\n",
		  limit, fdt_size, base, buf_size, image->size,
		  margin >= 0 ? " " : " -", abs(margin));
	if (margin < 0) {
		log_err("Image size %x but buffer is only %x\n", image->size,
			buf_size);
		return -ENOSPC;
	}

	rcode_buf = map_sysmem(rcode_base, rcode_size);
// 	print_buffer(0xff8c2000, (void *)0xff8c2000, 4, 0x100, 0);
	printf("_rcode_start %p: %x -- func %p %x\n", _rcode_start,
	       *(uint *)_rcode_start, setup_layout, *(uint *)setup_layout);
	print_buffer((ulong)_rcode_start, _rcode_start, 4, 4, 0);

	image->reloc_offset = rcode_buf - _rcode_start;
	log_debug("_rcode start %lx base %lx size %x offset %lx\n",
		  (ulong)map_to_sysmem(_rcode_start), rcode_base, rcode_size,
		  image->reloc_offset);

	memcpy(rcode_buf, _rcode_start, rcode_size);
	print_buffer(rcode_base, rcode_buf, 4, 4, 0);

	image->buf = map_sysmem(base, image->size);
	*addrp = base;

	return 0;
}

int spl_reloc_prepare(struct spl_image_info *image, ulong *addrp)
{
	int ret;
// 	volatile uint *ptr, *end, old, new;

	ret = setup_layout(image, addrp);
	if (ret)
		return ret;
/*
	ptr = (uint *)0xff8c0000;
	end = ptr + 0x2000;
	while (ptr < end) {
		old = readl(ptr);
		new = readl(ptr) ^ 0xffffffff;
		writel(new, ptr);
		printf("%p: %x: %x: %s", ptr, old, readl(ptr),
		       readl(ptr) == new ? "can write" : "no write");
		writel(old, ptr);
		printf(": %s\n", readl(ptr) == old ? "can restore" : "bad");
		ptr++;
	}
*/
	return 0;
}

void __rcode rcode_reloc_and_jump(struct spl_image_info *image,
				  spl_jump_to_image_t func)
{
	uint *src, *end, *dst;

	return;
	log_debug("Copying image size %lx from %x to %lx\n",
		  (ulong)map_to_sysmem(image->buf), image->size,
		  image->load_addr);
	for (dst = map_sysmem(image->load_addr, image->size),
	     src = image->buf, end = src + image->size / 4;
	     src < end;)
	     *dst++ = *src++;
}

int spl_reloc_jump(struct spl_image_info *image, spl_jump_to_image_t jump)
{
	rcode_func func;

	log_debug("reloc entry, stack_prot at %p\n", image->stack_prot);
	if (*image->stack_prot != STACK_PROT_VALUE) {
		log_err("stack busted, cannot continue\n");
		return -EFAULT;
	}
	func = (rcode_func)(void *)rcode_reloc_and_jump + image->reloc_offset;
	log_debug("Jumping to %p for %p\n", func, jump);
// 	print_buffer((ulong)func, func, 4, 0x10, 0);
	print_buffer(map_to_sysmem(image->buf), image->buf, 4, 0x10, 0);

	printf("\ndest:\n");
	print_buffer(image->load_addr,
		     map_sysmem(image->load_addr, image->size), 4, 0x10, 0);
	printf("hanging\n");
	while (1);
// 	func(image, jump);

	return 0;
}

#endif /* _SPL_RELOC_H */
