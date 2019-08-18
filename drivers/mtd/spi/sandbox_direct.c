// SPDX-License-Identifier: GPL-2.0+
/*
 * Simulate a flash chip without an underlying SPI layer. Behaviour is only
 * useful for testing.
 *
 * Copyright 2019 Google LLC
 *
 * Licensed under the GPL-2 or later.
 */

#define LOG_CATEGORY UCLASS_SPI_FLASH

#include <common.h>
#include <dm.h>
#include <spi_flash.h>

/**
 * struct sandbox_direct_priv - private data for this driver
 *
 * @read_byte: Byte to return when reading from the driver
 */
struct sandbox_direct_priv {
	char read_byte;
	int write_prot;
};

static int sandbox_direct_read(struct udevice *dev, u32 offset, size_t len,
			       void *buf)
{
	struct sandbox_direct_priv *priv = dev_get_priv(dev);

	if (offset == 1)
		return -EIO;
	memset(buf, priv->read_byte, len);

	return 0;
}

static int sandbox_direct_write(struct udevice *dev, u32 offset, size_t len,
				const void *buf)
{
	struct sandbox_direct_priv *priv = dev_get_priv(dev);

	if (offset == 1)
		return -EIO;
	if (len > 0)
		priv->read_byte = *(u8 *)buf;

	return 0;
}

static int sandbox_direct_erase(struct udevice *dev, u32 offset, size_t len)
{
	struct sandbox_direct_priv *priv = dev_get_priv(dev);

	if (offset == 1)
		return -EIO;
	if (len > 0)
		priv->read_byte = 'c';

	return 0;
}

static int sandbox_direct_get_sw_write_prot(struct udevice *dev)
{
	struct sandbox_direct_priv *priv = dev_get_priv(dev);

	return priv->write_prot++ ? 1 : 0;
}

static int sandbox_direct_get_mmap(struct udevice *dev, ulong *map_basep,
				   size_t *map_sizep, u32 *offsetp)
{
	*map_basep = 0x1000;
	*map_sizep = 0x2000;
	*offsetp = 0x100;

	return 0;
}

static int sandbox_direct_probe(struct udevice *dev)
{
	struct sandbox_direct_priv *priv = dev_get_priv(dev);

	priv->read_byte = 'a';

	return 0;
}

static struct dm_spi_flash_ops sandbox_direct_ops = {
	.read = sandbox_direct_read,
	.write = sandbox_direct_write,
	.erase = sandbox_direct_erase,
	.get_sw_write_prot = sandbox_direct_get_sw_write_prot,
	.get_mmap = sandbox_direct_get_mmap,
};

static const struct udevice_id sandbox_direct_ids[] = {
	{ .compatible = "sandbox,spi-flash-direct" },
	{ }
};

U_BOOT_DRIVER(sandbox_sf_direct) = {
	.name		= "sandbox_sf_direct",
	.id		= UCLASS_SPI_FLASH,
	.of_match	= sandbox_direct_ids,
	.probe		= sandbox_direct_probe,
	.ops		= &sandbox_direct_ops,
	.priv_auto_alloc_size	= sizeof(struct sandbox_direct_priv),
};
