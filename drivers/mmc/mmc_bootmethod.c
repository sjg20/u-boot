/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>

static int mmc_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	return -ENOSYS;
}

struct bootmethod_ops mmc_bootmethod_ops = {
	.get_bootflow	= mmc_get_bootflow,
};

U_BOOT_DRIVER(mmc_bootmethod) = {
	.name		= "mmc_bootmethod",
	.id		= UCLASS_BOOTMETHOD,
	.ops		= &mmc_bootmethod_ops,
};
