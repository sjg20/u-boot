// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <env_internal.h>
#include <malloc.h>
#include <dm/uclass-internal.h>

DECLARE_GLOBAL_DATA_PTR;

int bootmeth_check(struct udevice *dev, struct bootflow_iter *iter)
{
	const struct bootmeth_ops *ops = bootmeth_get_ops(dev);

	if (!ops->check)
		return 0;

	return ops->check(dev, iter);
}

int bootmeth_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmeth_ops *ops = bootmeth_get_ops(dev);

	if (!ops->read_bootflow)
		return -ENOSYS;

	return ops->read_bootflow(dev, bflow);
}

int bootmeth_boot(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmeth_ops *ops = bootmeth_get_ops(dev);

	if (!ops->boot)
		return -ENOSYS;

	return ops->boot(dev, bflow);
}

int bootmeth_read_file(struct udevice *dev, struct bootflow *bflow,
		       const char *file_path, ulong addr, ulong *sizep)
{
	const struct bootmeth_ops *ops = bootmeth_get_ops(dev);

	if (!ops->read_file)
		return -ENOSYS;

	return ops->read_file(dev, bflow, file_path, addr, sizep);
}

int bootmeth_set_order(const char *order_str)
{
	struct bootstd_priv *std;
	struct udevice **order;
	int count, ret, i, len;
	const char *s, *p;

	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;

	if (!order_str) {
		free(std->bootmeth_order);
		std->bootmeth_order = NULL;
		std->bootmeth_count = 0;
		return 0;
	}

	/* Create an array large enough */
	count = uclass_id_count(UCLASS_BOOTMETH);
	if (!count)
		return log_msg_ret("count", -ENOENT);

	order = calloc(count + 1, sizeof(struct udevice *));
	if (!order)
		return log_msg_ret("order", -ENOMEM);

	for (i = 0, s = order_str; *s && i < count; s = p + (*p == ','), i++) {
		struct udevice *dev;

		p = strchrnul(s, ',');
		len = p - s;
		ret = uclass_find_device_by_namelen(UCLASS_BOOTMETH, s, len,
						    &dev);
		if (ret) {
			printf("Unknown bootmeth '%.*s'\n", len, s);
			free(order);
			return ret;
		}
		order[i] = dev;
	}
	order[i] = NULL;
	free(std->bootmeth_order);
	std->bootmeth_order = order;
	std->bootmeth_count = i;

	return 0;
}

#ifdef CONFIG_BOOTSTD_FULL
/**
 * on_bootmeths() - Update the bootmeth order
 *
 * This will check for a valid baudrate and only apply it if valid.
 */
static int on_bootmeths(const char *name, const char *value, enum env_op op,
	int flags)
{
	int ret;

	switch (op) {
	case env_op_create:
	case env_op_overwrite:
		ret = bootmeth_set_order(value);
		if (ret)
			return 1;
		return 0;
	case env_op_delete:
		bootmeth_set_order(NULL);
		fallthrough;
	default:
		return 0;
	}
}
U_BOOT_ENV_CALLBACK(bootmeths, on_bootmeths);
#endif /* CONFIG_BOOTSTD_FULL */

UCLASS_DRIVER(bootmeth) = {
	.id		= UCLASS_BOOTMETH,
	.name		= "bootmeth",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_plat_auto	= sizeof(struct bootmeth_uc_plat),
};
