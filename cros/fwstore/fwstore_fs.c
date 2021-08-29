// SPDX-License-Identifier: GPL-2.0+
/*
 * Allows access to 'firmware storage' on an EFI partition filesystem
 *
 * Copyright 2021 Google LLC
 */

#define LOG_CATEGORY UCLASS_CROS_FWSTORE

#include <common.h>
#include <blk.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include <mapmem.h>
#include <cros/cros_common.h>
#include <cros/cros_ofnode.h>
#include <cros/fwstore.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * @dev: Media device to read/write
 * @part: Partition number on that device (0=whole device, 1=partition 1)
 * @filename: Filename within filesystem
 */
struct fs_priv {
	struct udevice *dev;
	int part;
	const char *filename;
};

static int get_fs(struct fs_priv *priv)
{
	struct blk_desc *blk;
	int ret;

	blk = blk_get_by_device(priv->dev);
	if (!blk)
		return log_msg_ret("blk", -ENODEV);

	ret = fs_set_blk_dev_with_part(blk, priv->part);
	if (ret)
		return log_msg_ret("set", ret);

	return 0;
}

static int fwstore_fs_read(struct udevice *dev, ulong offset, ulong count,
			    void *buf)
{
	struct fs_priv *priv = dev_get_priv(dev);
	loff_t actual;
	ulong addr;
	int ret;

	ret = get_fs(priv);
	if (ret)
		return log_msg_ret("get", ret);
	if (!count)
		return 0;
	addr = map_to_sysmem(buf);
	ret = fs_read(priv->filename, addr, offset, count, &actual);
	if (ret)
		return log_msg_ret("read", ret);
	if (count != actual)
		return log_msg_ret("count", -EIO);

	return 0;
}

/*
 * Does not support unaligned writes.
 * Offset and count must be offset-aligned.
 */
static int fwstore_fs_write(struct udevice *dev, ulong offset, ulong count,
			     void *buf)
{
	struct fs_priv *priv = dev_get_priv(dev);
	loff_t actual;
	ulong addr;
	int ret;

	ret = get_fs(priv);
	if (ret)
		return log_msg_ret("get", ret);
	addr = map_to_sysmem(buf);
	ret = fs_write(priv->filename, addr, offset, count, &actual);
	if (ret)
		return log_msg_ret("read", ret);
	if (count != actual)
		return log_msg_ret("count", -EIO);

	return 0;
}

static int fwstore_fs_sw_wp_enabled_fs(struct udevice *dev)
{
	return false;
}

static int fwstore_fs_of_to_plat(struct udevice *dev)
{
	struct fs_priv *priv = dev_get_priv(dev);
	int ret;

	ret = dev_read_u32(dev, "partition", &priv->part);
	if (ret)
		return log_msg_ret("part", ret);
	priv->filename = dev_read_string(dev, "filename");
	if (!priv->filename)
		return log_msg_ret("fname", ret);

	return 0;
}

static int fwstore_fs_probe(struct udevice *dev)
{
	struct fs_priv *priv = dev_get_priv(dev);
	int ret;

	/* For now, use the first available device */
	ret = uclass_first_device_err(UCLASS_EFI_MEDIA, &priv->dev);
	if (ret)
		return log_msg_ret("dev", ret);

	return 0;
}

static const struct cros_fwstore_ops fwstore_fs_ops = {
	.read		= fwstore_fs_read,
	.write		= fwstore_fs_write,
	.sw_wp_enabled	= fwstore_fs_sw_wp_enabled_fs,
};

static struct udevice_id fwstore_fs_ids[] = {
	{ .compatible = "google,fwstore-fs" },
	{ },
};

U_BOOT_DRIVER(fwstore_fs) = {
	.name	= "fwstore_fs",
	.id	= UCLASS_CROS_FWSTORE,
	.of_match = fwstore_fs_ids,
	.ops	= &fwstore_fs_ops,
	.of_to_plat	= fwstore_fs_of_to_plat,
	.probe	= fwstore_fs_probe,
	.priv_auto = sizeof(struct fs_priv),
};
