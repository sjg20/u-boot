/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, daniel@omicron.se
 */

#ifndef _ASM_ZIMAGE_H_
#define _ASM_ZIMAGE_H_

#include <asm/bootparam.h>
#include <asm/e820.h>

/* linux i386 zImage/bzImage header. Offsets relative to
 * the start of the image */

#define HEAP_FLAG           0x80
#define BIG_KERNEL_FLAG     0x01

/* magic numbers */
#define KERNEL_MAGIC        0xaa55
#define KERNEL_V2_MAGIC     0x53726448
#define COMMAND_LINE_MAGIC  0xA33F

/* limits */
#define BZIMAGE_MAX_SIZE   15*1024*1024     /* 15MB */
#define ZIMAGE_MAX_SIZE    512*1024         /* 512k */
#define SETUP_MAX_SIZE     32768

#define SETUP_START_OFFSET 0x200
#define BZIMAGE_LOAD_ADDR  0x100000
#define ZIMAGE_LOAD_ADDR   0x10000

/**
 * load_zimage() - Load a zImage or bzImage
 *
 * This copies an image into the standard location ready for setup
 *
 * @image: Address of image to load
 * @kernel_size: Size of kernel including setup block (or 0 if the kernel is
 *	new enough to have a 'syssize' value)
 * @load_addressp: Returns the address where the kernel has been loaded
 * Return: address of setup block, or NULL if something went wrong
 */
struct boot_params *load_zimage(char *image, unsigned long kernel_size,
				ulong *load_addressp);

#endif
