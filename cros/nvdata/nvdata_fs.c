// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_NVDATA

#include <common.h>
#include <blk.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include <mapmem.h>
#include <cros/nvdata.h>

#define VBOOT_HASH_VSLOT	0
#define VBOOT_HASH_VSLOT_MASK	(1 << (VBOOT_HASH_VSLOT))

/**
 * @dev: Media device to read/write
 * @part: Partition number on that device (0=whole device, 1=partition 1)
 */
struct fs_nvdata_priv {
	struct udevice *dev;
	int part;
};

static int get_fs(struct fs_nvdata_priv *priv)
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

static void get_nvdata_filename(enum cros_nvdata_type type, char *fname,
				int size)
{
	const char *name = cros_nvdata_name(type);

	if (name)
		strlcpy(fname, name, size);
	else
		snprintf(fname, size, "nvd%x", type);
}

static int fs_nvdata_read(struct udevice *dev, enum cros_nvdata_type type,
			       u8 *data, int size)
{
	struct fs_nvdata_priv *priv = dev_get_priv(dev);
	char fname[20];
	loff_t actual;
	ulong addr;
	int ret;

	ret = get_fs(priv);
	if (ret)
		return log_msg_ret("get", ret);
	addr = map_to_sysmem(data);
	get_nvdata_filename(type, fname, sizeof(fname));
	ret = fs_read(fname, addr, 0, size, &actual);
	if (ret) {
		if (ret != -ENOENT)
			return log_msg_ret("read", ret);

		/*
		 * If the file was not found, use zero data. At some point the
		 * data will be set up and then written in fs_nvdata_write(),
		 * for next time we boot
		 */
		memset(data, '\0', size);
		actual = size;
	}
	if (size != actual)
		return log_msg_ret("size", -EIO);

	return 0;
}

static int fs_nvdata_write(struct udevice *dev, enum cros_nvdata_type type,
				const u8 *data, int size)
{
	struct fs_nvdata_priv *priv = dev_get_priv(dev);
	char fname[20];
	loff_t actual;
	ulong addr;
	int ret;

	ret = get_fs(priv);
	if (ret)
		return log_msg_ret("get", ret);
	addr = map_to_sysmem(data);
	get_nvdata_filename(type, fname, sizeof(fname));
	ret = fs_write(fname, addr, 0, size, &actual);
	if (ret)
		return log_msg_ret("read", ret);
	if (size != actual)
		return log_msg_ret("size", -EIO);

	return 0;
}

static int fs_nvdata_lock(struct udevice *dev, enum cros_nvdata_type type)
{
	switch (type) {
	case CROS_NV_VSTORE:
		log_warning("Cannot handle vstore locking %x\n", type);
		return -EPERM;
	default:
		log_debug("Type %x not supported\n", type);
		return -ENOSYS;
	}

	return 0;
}

static int fs_nvdata_of_to_plat(struct udevice *dev)
{
	struct fs_nvdata_priv *priv = dev_get_priv(dev);
	int ret;

	ret = cros_nvdata_of_to_plat(dev);
	if (ret)
		return log_msg_ret("cros", ret);

	/* For now, use the first available device */
	ret = uclass_first_device_err(UCLASS_EFI_MEDIA, &priv->dev);
	if (ret)
		return log_msg_ret("dev", ret);
	ret = dev_read_u32(dev, "partition", &priv->part);
	if (ret)
		return log_msg_ret("part", ret);

	return 0;
}

static const struct cros_nvdata_ops fs_nvdata_ops = {
	.read	= fs_nvdata_read,
	.write	= fs_nvdata_write,
	.lock	= fs_nvdata_lock,
};

static const struct udevice_id fs_nvdata_ids[] = {
	{ .compatible = "google,fs-nvdata" },
	{ }
};

U_BOOT_DRIVER(google_fs_nvdata) = {
	.name		= "google_fs_nvdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= fs_nvdata_ids,
	.ops		= &fs_nvdata_ops,
	.of_to_plat	= fs_nvdata_of_to_plat,
	.priv_auto	= sizeof(struct fs_nvdata_priv),
};
