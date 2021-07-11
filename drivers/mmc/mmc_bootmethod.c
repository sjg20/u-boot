/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>
#include <mmc.h>
#include <dm/device-internal.h>

static int mmc_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	struct udevice *mmc_dev = dev_get_parent(dev);
	struct udevice *blk;
	int ret;

	ret = mmc_get_blk(mmc_dev, &blk);
	if (ret)
		return log_msg_ret("blk", ret);
	assert(blk);
	ret = bootmethod_find_in_blk(dev, blk, seq, bflow);
	if (ret)
		return log_msg_ret("find", ret);

	return 0;
}

struct bootmethod_ops mmc_bootmethod_ops = {
	.get_bootflow	= mmc_get_bootflow,
};

U_BOOT_DRIVER(mmc_bootmethod) = {
	.name		= "mmc_bootmethod",
	.id		= UCLASS_BOOTMETHOD,
	.ops		= &mmc_bootmethod_ops,
};
