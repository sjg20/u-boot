// SPDX-License-Identifier: GPL-2.0+
/*
 * Uclass implemenation for standard boot
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootstd.h>
#include <dm.h>
#include <dm/read.h>

static int bootstd_of_to_plat(struct udevice *dev)
{
	struct bootstd_plat *plat = dev_get_plat(dev);

	plat->prefixes = dev_read_string_list(dev, "filename-prefixes");
	plat->order = dev_read_string_list(dev, "bootmeth-order");

	return 0;
}

static const struct udevice_id bootstd_ids[] = {
	{ .compatible = "u-boot,boot-standard" },
	{ }
};

U_BOOT_DRIVER(bootstd_drv) = {
	.id		= UCLASS_BOOTSTD,
	.name		= "bootstd_drv",
	.of_to_plat	= bootstd_of_to_plat,
	.of_match	= bootstd_ids,
	.plat_auto	= sizeof(struct bootstd_plat),
};

UCLASS_DRIVER(bootstd) = {
	.id		= UCLASS_BOOTSTD,
	.name		= "bootstd",
	.post_bind	= dm_scan_fdt_dev,
};
