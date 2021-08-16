/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __distro_h
#define __distro_h

struct blk_desc;

int distro_boot_setup(struct blk_desc *desc, int partnum,
		      struct bootflow *bflow);

int distro_net_setup(struct bootflow *bflow);

int distro_boot(struct bootflow *bflow);

#endif
