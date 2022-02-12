// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootdevice for MMC
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <dm.h>

static int host_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			     struct bootflow *bflow)
{
	printf("get\n");

	return 0;
}

struct bootdev_ops host_bootdev_ops = {
	.get_bootflow	= host_get_bootflow,
};

static const struct udevice_id host_bootdev_ids[] = {
	{ .compatible = "sandbox,bootdev-host" },
	{ }
};

U_BOOT_DRIVER(host_bootdev) = {
	.name		= "host_bootdev",
	.id		= UCLASS_BOOTDEV,
	.ops		= &host_bootdev_ops,
	.of_match	= host_bootdev_ids,
};
