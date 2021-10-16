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
#include <malloc.h>
#include <dm/read.h>

static int bootstd_of_to_plat(struct udevice *dev)
{
	struct bootstd_priv *priv = dev_get_priv(dev);
	int ret;

	ret = dev_read_string_list(dev, "filename-prefixes", &priv->prefixes);
	if (ret < 0 && ret != -ENOENT)
		return log_msg_ret("fname", ret);
	ret = dev_read_string_list(dev, "bootmeth-order", &priv->order);
	if (ret < 0 && ret != -ENOENT)
		return log_msg_ret("order", ret);

	return 0;
}

static int bootstd_remove(struct udevice *dev)
{
	struct bootstd_priv *priv = dev_get_priv(dev);

	free(priv->prefixes);
	free(priv->order);

	return 0;
}

const char **bootstd_get_order(struct udevice *dev)
{
	struct bootstd_priv *priv = dev_get_priv(dev);

	return priv->order;
}

static const struct udevice_id bootstd_ids[] = {
	{ .compatible = "u-boot,boot-standard" },
	{ }
};

U_BOOT_DRIVER(bootstd_drv) = {
	.id		= UCLASS_BOOTSTD,
	.name		= "bootstd_drv",
	.of_to_plat	= bootstd_of_to_plat,
	.remove		= bootstd_remove,
	.of_match	= bootstd_ids,
	.priv_auto	= sizeof(struct bootstd_priv),
};

UCLASS_DRIVER(bootstd) = {
	.id		= UCLASS_BOOTSTD,
	.name		= "bootstd",
	.post_bind	= dm_scan_fdt_dev,
};
