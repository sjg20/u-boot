/* SPDX-License-Identifier: Intel */
/*
 * Copyright (C) 2015-2016 Intel Corp.
 * (Written by Alexandru Gagniuc <alexandrux.gagniuc@intel.com> for Intel Corp.)
 * Mostly taken from coreboot
 */

#ifndef __ASM_FSP_INTERNAL_H
#define __ASM_FSP_INTERNAL_H

struct fsp_header;

int fsp_get_header(ulong offset, ulong size, bool use_spi_flash,
		   struct fsp_header **fspp, ulong *basep);

#endif
