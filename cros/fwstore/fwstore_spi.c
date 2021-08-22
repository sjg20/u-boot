// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of firmware storage access interface for SPI flash
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY UCLASS_CROS_FWSTORE

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <spi.h>
#include <spi_flash.h>
#include <cros/cros_common.h>
#include <cros/cros_ofnode.h>
#include <cros/fwstore.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * struct fwstore_spi_priv - private information for the driver
 *
 * @sf: SPI flash device that this driver uses
 * @rom_offset: Address of offset 0 of memory-mapped ROM (0 if none)
 */
struct fwstore_spi_priv {
	struct udevice *sf;
	u32 rom_offset;
};

/**
 * border_check() - Check if an offset/count are in range
 *
 * Return 0 if the region is is within range, -ESPIPE if the start offset is
 * beyond the end of the device, -ERANGE if the region extends beyond the
 * device
 */
static int border_check(struct udevice *sf, uint offset, uint count)
{
	struct spi_flash *flash = dev_get_uclass_priv(sf);
	uint max_offset = offset + count;

	if (offset >= flash->size) {
		log_debug("at EOF: offset=%x, size=%x\n", offset, flash->size);
		return log_msg_ret("eof", -ESPIPE);
	}

	/* max_offset will be less than offset iff overflow occurred */
	if (max_offset < offset || max_offset > flash->size) {
		log_debug("exceed range offset=%x, max_offset=%x, flash->size=%x\n",
			  offset, max_offset, flash->size);
		return log_msg_ret("range", -ERANGE);
	}

	return 0;
}

static int fwstore_spi_read(struct udevice *dev, ulong offset, ulong count,
			    void *buf)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	int ret;

	ret = border_check(priv->sf, offset, count);
	if (ret)
		return ret;

	ret = spi_flash_read_dm(priv->sf, offset, count, buf);
	if (ret) {
		log_debug("SPI read fail (count=%ld, ret=%d)\n", count, ret);
		return ret;
	}

	return 0;
}

/**
 * align_to_sector() - Align offset and size
 *
 * Align the right-exclusive range [*offsetp:*offsetp+*lengthp) with
 * the sector size.
 * After alignment adjustment, both offset and length will be multiple of
 * the sector, and will be larger than or equal to the original range.
 *
 * @sector_size: Sector size in bytes (e.g. 4096)
 * @offsetp: Pointer to offset to update
 * @lengthp: Pointer to length to update
 */
static void align_to_sector(uint sector_size, uint *offsetp, uint *lengthp)
{
	log_debug("before adjustment\n");
	log_debug("offset: 0x%x\n", *offsetp);
	log_debug("length: 0x%x\n", *lengthp);

	/* Adjust if offset is not multiple of sector_size */
	if (*offsetp & (sector_size - 1))
		*offsetp &= ~(sector_size - 1);

	/* Adjust if length is not multiple of sector_size */
	if (*lengthp & (sector_size - 1)) {
		*lengthp &= ~(sector_size - 1);
		*lengthp += sector_size;
	}

	log_debug("after adjustment\n");
	log_debug("offset: 0x%x\n", *offsetp);
	log_debug("length: 0x%x\n", *lengthp);
}

static int fwstore_spi_write(struct udevice *dev, ulong offset, ulong count,
			     void *buf)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	struct spi_flash *flash = dev_get_uclass_priv(priv->sf);
	u8 static_buf[flash->sector_size];
	u8 *backup_buf;
	uint pos, len;
	int ret;

	/* We will erase <n> bytes starting from <pos> */
	pos = offset;
	len = count;
	align_to_sector(flash->sector_size, &pos, &len);

	log_debug("offset:          %08lx\n", offset);
	log_debug("adjusted offset: %08x\n", pos);
	if (pos > offset) {
		log_debug("align incorrect: %08x > %08lx\n", pos, offset);
		return log_msg_ret("aligh", -EINVAL);
	}

	if (border_check(priv->sf, pos, len))
		return log_msg_ret("border", -ERANGE);

	backup_buf = len > sizeof(static_buf) ? malloc(len) : static_buf;
	if (!backup_buf)
		return log_msg_ret("Cannot alloc fwstore tmp buf", -ENOMEM);

	ret = spi_flash_read_dm(priv->sf, pos, len, backup_buf);
	if (ret) {
		log_err("cannot backup data: %d\n", ret);
		goto exit;
	}

	ret = spi_flash_erase_dm(priv->sf, pos, len);
	if (ret) {
		log_err("SPI erase fail: %d\n", ret);
		goto exit;
	}

	/* combine data we want to write and backup data */
	memcpy(backup_buf + (offset - pos), buf, count);

	ret = spi_flash_write_dm(priv->sf, pos, len, backup_buf);
	if (ret) {
		log_err("SPI write fail: %d\n", ret);
		goto exit;
	}

	ret = 0;

exit:
	if (backup_buf != static_buf)
		free(backup_buf);

	return ret;
}

static int fwstore_spi_get_sw_write_prot(struct udevice *dev)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	int ret;

	ret = spl_flash_get_sw_write_prot(priv->sf);
	if (ret < 0) {
		log_warning("spl_flash_get_write_prot_dm() failed: %d\n", ret);
		return 0;
	}
	log_debug("flash SW WP is %d\n", ret);

	return ret != 0;
}

static int fwstore_spi_mmap(struct udevice *dev, uint offset, uint size,
			    ulong *addrp)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);

	if (!priv->rom_offset)
		return log_msg_ret("no", -ENOSYS);
	*addrp = offset + priv->rom_offset;
	log_buffer(LOGC_VBOOT, LOGL_DEBUG, *addrp, (void *)*addrp, 1, 0x20, 0);

	return 0;
}

/**
 * setup_mmap() - Set up the memory map for the ROM
 *
 * On x86 systems the SPI flash can be memory-mapped. This figures out the
 * mapping and sets up priv->rom_offset so that it can be used.
 *
 * @dev: fwstore device
 * @return 0 if OK, -ENOSYS if not supported, other -ve value on other error
 */
static int setup_mmap(struct udevice *dev)
{
#ifdef CONFIG_ROM_SIZE
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	ulong mask = CONFIG_ROM_SIZE - 1;
	uint map_size;
	ulong map_base;
	uint mem_offset;
	int ret;

	/* Use the SPI driver to get the memory map */
	ret = dm_spi_get_mmap(priv->sf, &map_base, &map_size, &mem_offset);
	if (ret)
		return log_msg_ret("Could not get flash mmap", ret);
	priv->rom_offset = (map_base & mask) - CONFIG_ROM_SIZE;

	return 0;
#else
	return -ENOSYS;
#endif
}

int fwstore_spi_probe(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	return log_msg_ret("of", -EINVAL);
#else
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	struct ofnode_phandle_args args;
	int ret;

	log_debug("init %s\n", dev->name);
	ret = dev_read_phandle_with_args(dev, "firmware-storage", NULL, 0, 0,
					 &args);
	if (ret < 0) {
		log_debug("fail to look up phandle for device %s\n", dev->name);
		return log_msg_ret("phandle", ret);
	}

	ret = uclass_get_device_by_ofnode(UCLASS_SPI_FLASH, args.node,
					  &priv->sf);
	if (ret) {
		log_debug("fail to init SPI flash at %s: %s: ret=%d\n",
			  dev->name, ofnode_get_name(args.node), ret);
		return log_msg_ret("init", ret);
	}

	/* Use the devicetree offset if available, otherwise find it */
	if (dev_read_u32(dev, "rom-offset", &priv->rom_offset))
		setup_mmap(dev);
#endif

	return 0;
}

static const struct cros_fwstore_ops fwstore_spi_ops = {
	.read		= fwstore_spi_read,
	.write		= fwstore_spi_write,
	.sw_wp_enabled	= fwstore_spi_get_sw_write_prot,
	.mmap		= fwstore_spi_mmap,
};

static struct udevice_id fwstore_spi_ids[] = {
	{ .compatible = "cros,fwstore-spi" },
	{ },
};

U_BOOT_DRIVER(cros_fwstore_spi) = {
	.name	= "cros_fwstore_spi",
	.id	= UCLASS_CROS_FWSTORE,
	.of_match = fwstore_spi_ids,
	.ops	= &fwstore_spi_ops,
	.probe	= fwstore_spi_probe,
	.priv_auto = sizeof(struct fwstore_spi_priv),
};
