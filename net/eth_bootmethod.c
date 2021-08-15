// /* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Bootmethod for ethernet
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>
#include <net.h>

static int eth_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	struct udevice *eth_dev = dev_get_parent(dev);
	struct udevice *blk;
	int ret;

	ret = eth_get_blk(eth_dev, &blk);
	if (ret)
		return log_msg_ret("blk", ret);
	assert(blk);
	ret = bootmethod_find_in_blk(dev, blk, seq, bflow);
	if (ret)
		return log_msg_ret("find", ret);

	return 0;
}

struct bootmethod_ops eth_bootmethod_ops = {
	.get_bootflow	= eth_get_bootflow,
};

U_BOOT_DRIVER(eth_bootmethod) = {
	.name		= "eth_bootmethod",
	.id		= UCLASS_BOOTMETHOD,
	.ops		= &eth_bootmethod_ops,
};
