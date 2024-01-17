/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Verified Boot for Embedded (VBE) common functions
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __VBE_COMMON_H
#define __VBE_COMMON_H

#include <linux/types.h>

struct spl_load_info;

ulong h_vbe_load_read(struct spl_load_info *load, ulong off, ulong size,
		      void *buf);

#endif /* __VBE_ABREC_H */
