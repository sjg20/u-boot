// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <dm.h>

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

int bootdev_get_state(struct bootdev_state **statep)
{
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_BOOTDEV, &uc);
	if (ret)
		return ret;
	*statep = uclass_get_priv(uc);

	return 0;
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

static void bootdev_clear_glob_(struct bootdev_state *state)
{
	while (!list_empty(&state->glob_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&state->glob_head, struct bootflow,
					 glob_node);
		bootflow_remove(bflow);
	}
}

void bootdev_clear_glob(void)
{
	struct bootdev_state *state;

	if (bootdev_get_state(&state))
		return;

	bootdev_clear_glob_(state);
}

static int bootdev_init(struct uclass *uc)
{
	struct bootdev_state *state = uclass_get_priv(uc);

	INIT_LIST_HEAD(&state->glob_head);

	return 0;
}

static int bootdev_destroy(struct uclass *uc)
{
	struct bootdev_state *state = uclass_get_priv(uc);

	bootdev_clear_glob_(state);

	return 0;
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
	.priv_auto	= sizeof(struct bootdev_state),
	.per_device_plat_auto	= sizeof(struct bootdev_uc_plat),
	.init		= bootdev_init,
	.destroy	= bootdev_destroy,
	.post_bind	= bootdev_post_bind,
	.pre_unbind	= bootdev_pre_unbind,
};
