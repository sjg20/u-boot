// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmeth.h>
#include <dm.h>

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

UCLASS_DRIVER(bootmeth) = {
	.id		= UCLASS_BOOTMETH,
	.name		= "bootmeth",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_plat_auto	= sizeof(struct bootmeth_uc_plat),
};
