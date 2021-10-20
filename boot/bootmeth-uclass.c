// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmeth.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>

DECLARE_GLOBAL_DATA_PTR;

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

/* For now, bind the boormethod device if none are found in the devicetree */
int dm_scan_other(bool pre_reloc_only)
{
	struct driver *drv = ll_entry_start(struct driver, driver);
	const int n_ents = ll_entry_count(struct driver, driver);
	struct udevice *dev, *bootstd;
	int i, ret;

	/*
	 * If there is a bootstd device, skip, since we assume that the bootmeth
	 * devices have been created correctly.
	 */
	uclass_find_first_device(UCLASS_BOOTSTD, &bootstd);
	if (bootstd)
		return 0;

	ret = device_bind_driver(gd->dm_root, "bootstd_drv", "bootstd",
				 &bootstd);
	if (ret)
		return log_msg_ret("bootstd", ret);

	for (i = 0; i < n_ents; i++, drv++) {
		/*
		 * Disable EFI Manager for now as no one uses it so it is
		 * confusing
		 */
		if (drv->id == UCLASS_BOOTMETH &&
		    strcmp("efi_mgr_bootmeth", drv->name)) {
			ret = device_bind(bootstd, drv, drv->name, 0,
					  ofnode_null(), &dev);
			if (ret)
				return log_msg_ret("bind", ret);
		}
	}

	return 0;
}

UCLASS_DRIVER(bootmeth) = {
	.id		= UCLASS_BOOTMETH,
	.name		= "bootmeth",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_plat_auto	= sizeof(struct bootmeth_uc_plat),
};
