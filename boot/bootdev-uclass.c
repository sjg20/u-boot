// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootstd.h>

int bootdev_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			 struct bootflow *bflow)
{
	const struct bootdev_ops *ops = bootdev_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;
	memset(bflow, '\0', sizeof(*bflow));
	bflow->dev = dev;
	bflow->method = iter->method;

	return ops->get_bootflow(dev, iter, bflow);
}

void bootdev_clear_bootflows(struct udevice *dev)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	while (!list_empty(&ucp->bootflow_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&ucp->bootflow_head, struct bootflow,
					 bm_node);
		bootflow_remove(bflow);
	}
}

static int bootdev_post_bind(struct udevice *dev)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	INIT_LIST_HEAD(&ucp->bootflow_head);

	return 0;
}

static int bootdev_pre_unbind(struct udevice *dev)
{
	bootdev_clear_bootflows(dev);

	return 0;
}

UCLASS_DRIVER(bootdev) = {
	.id		= UCLASS_BOOTDEV,
	.name		= "bootdev",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_plat_auto	= sizeof(struct bootdev_uc_plat),
	.post_bind	= bootdev_post_bind,
	.pre_unbind	= bootdev_pre_unbind,
};
