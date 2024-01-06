/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _SPL_RELOC_H
#define _SPL_RELOC_H

#include <spl.h>
#include <linux/types.h>

void spl_reloc_prepare(struct spl_image_info *image, ulong *addrp)
{
}

void spl_reloc_jump(struct spl_image_info *image, spl_jump_to_image_t func)
{
}

#endif /* _SPL_RELOC_H */
