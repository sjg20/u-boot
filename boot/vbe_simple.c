// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified Boot for Embedded (VBE) 'simple' method
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY LOGC_BOOT

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootstage.h>
#include <bootmeth.h>
#include <dm.h>
#include <mapmem.h>
#include <image.h>
#include <log.h>
#include <memalign.h>
#include <mmc.h>
#include <part.h>
#include <spl.h>
#include <vbe.h>
#include <version_string.h>
#include <dm/device-internal.h>
#include <dm/ofnode.h>
#include <u-boot/crc.h>

enum {
	MAX_VERSION_LEN		= 256,

	NVD_HDR_VER_SHIFT	= 0,
	NVD_HDR_VER_MASK	= 0xf,
	NVD_HDR_SIZE_SHIFT	= 4,
	NVD_HDR_SIZE_MASK	= 0xf << NVD_HDR_SIZE_SHIFT,

	/* Firmware key-version is in the top 16 bits of fw_ver */
	FWVER_KEY_SHIFT		= 16,
	FWVER_FW_MASK		= 0xffff,

	NVD_HDR_VER_CUR		= 1,	/* current version */
};

/** struct simple_priv - information read from the device tree */
struct simple_priv {
	u32 area_start;
	u32 area_size;
	u32 skip_offset;
	u32 state_offset;
	u32 state_size;
	u32 version_offset;
	u32 version_size;
	const char *storage;
};

/** struct simple_state - state information read from media
 *
 * @fw_version: Firmware version string
 * @fw_vernum: Firmware version number
 */
struct simple_state {
	char fw_version[MAX_VERSION_LEN];
	u32 fw_vernum;
};

/** struct simple_nvdata - storage format for non-volatile data */
struct simple_nvdata {
	u8 crc8;
	u8 hdr;
	u16 spare1;
	u32 fw_vernum;
	u8 spare2[0x38];
};

static int simple_read_version(struct udevice *dev, struct blk_desc *desc,
			       u8 *buf, struct simple_state *state)
{
	struct simple_priv *priv = dev_get_priv(dev);
	int start;

	if (priv->version_size > MMC_MAX_BLOCK_LEN)
		return log_msg_ret("ver", -E2BIG);

	start = priv->area_start + priv->version_offset;
	if (start & (MMC_MAX_BLOCK_LEN - 1))
		return log_msg_ret("get", -EBADF);
	start /= MMC_MAX_BLOCK_LEN;

	if (blk_dread(desc, start, 1, buf) != 1)
		return log_msg_ret("read", -EIO);
	strlcpy(state->fw_version, buf, MAX_VERSION_LEN);
	log_debug("version=%s\n", state->fw_version);

	return 0;
}

static int simple_read_nvdata(struct udevice *dev, struct blk_desc *desc,
			      u8 *buf, struct simple_state *state)
{
	struct simple_priv *priv = dev_get_priv(dev);
	uint hdr_ver, hdr_size, size, crc;
	const struct simple_nvdata *nvd;
	int start;

	if (priv->state_size > MMC_MAX_BLOCK_LEN)
		return log_msg_ret("state", -E2BIG);

	start = priv->area_start + priv->state_offset;
	if (start & (MMC_MAX_BLOCK_LEN - 1))
		return log_msg_ret("get", -EBADF);
	start /= MMC_MAX_BLOCK_LEN;

	if (blk_dread(desc, start, 1, buf) != 1)
		return log_msg_ret("read", -EIO);
	nvd = (struct simple_nvdata *)buf;
	hdr_ver = (nvd->hdr & NVD_HDR_VER_MASK) >> NVD_HDR_VER_SHIFT;
	hdr_size = (nvd->hdr & NVD_HDR_SIZE_MASK) >> NVD_HDR_SIZE_SHIFT;
	if (hdr_ver != NVD_HDR_VER_CUR)
		return log_msg_ret("hdr", -EPERM);
	size = 1 << hdr_size;
	if (size > sizeof(*nvd))
		return log_msg_ret("sz", -ENOEXEC);

	crc = crc8(0, buf + 1, size - 1);
	if (crc != nvd->crc8)
		return log_msg_ret("crc", -EPERM);
	state->fw_vernum = nvd->fw_vernum;

	log_debug("version=%s\n", state->fw_version);

	return 0;
}

static int simple_read_state(struct udevice *dev, struct simple_state *state)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, buf, MMC_MAX_BLOCK_LEN);
	struct simple_priv *priv = dev_get_priv(dev);
	struct blk_desc *desc;
	char devname[16];
	const char *end;
	int devnum;
	int ret;

	/* First figure out the block device */
	log_debug("storage=%s\n", priv->storage);
	devnum = trailing_strtoln_end(priv->storage, NULL, &end);
	if (devnum == -1)
		return log_msg_ret("num", -ENODEV);
	if (end - priv->storage >= sizeof(devname))
		return log_msg_ret("end", -E2BIG);
	strlcpy(devname, priv->storage, end - priv->storage + 1);
	log_debug("dev=%s, %x\n", devname, devnum);

	desc = blk_get_dev(devname, devnum);
	if (!desc)
		return log_msg_ret("get", -ENXIO);

	ret = simple_read_version(dev, desc, buf, state);
	if (ret)
		return log_msg_ret("ver", ret);

	ret = simple_read_nvdata(dev, desc, buf, state);
	if (ret)
		return log_msg_ret("nvd", ret);

	return 0;
}

static int vbe_simple_get_state_desc(struct udevice *dev, char *buf,
				     int maxsize)
{
	struct simple_state state;
	int ret;

	ret = simple_read_state(dev, &state);
	if (ret)
		return log_msg_ret("read", ret);

	if (maxsize < 30)
		return -ENOSPC;
	snprintf(buf, maxsize, "Version: %s\nVernum: %x/%x", state.fw_version,
		 state.fw_vernum >> FWVER_KEY_SHIFT,
		 state.fw_vernum & FWVER_FW_MASK);

	return 0;
}

static int vbe_simple_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	/* To be implemented */

	return -EINVAL;
}

static int vbe_simple_read_file(struct udevice *dev, struct bootflow *bflow,
				const char *file_path, ulong addr, ulong *sizep)
{
	int ret;

	if (vbe_phase() == VBE_PHASE_OS) {
		ret = bootmeth_common_read_file(dev, bflow, file_path, addr,
						sizep);
		if (ret)
			return log_msg_ret("os", ret);
	}

	/* To be implemented */
	return -EINVAL;
}

static int vbe_simple_read_fw_bootflow(struct udevice *bdev,
				       struct udevice *meth,
				       struct bootflow *bflow)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, sbuf, MMC_MAX_BLOCK_LEN);
	struct udevice *media = dev_get_parent(bdev);
	struct simple_priv *priv = dev_get_priv(meth);
	const char *fit_uname, *fit_uname_config;
	struct bootm_headers images = {};
	ulong offset, size, blknum, addr, len, load_addr, num_blks;
	struct blk_desc *desc;
	struct udevice *blk;
	void *buf;
	int ret;

	log_debug("media=%s\n", media->name);
	ret = blk_get_from_parent(media, &blk);
	if (ret)
		return log_msg_ret("med", ret);
	log_debug("blk=%s\n", blk->name);
	desc = dev_get_uclass_plat(blk);

	bootflow_init(bflow, bdev, meth);

	offset = priv->area_start + priv->skip_offset;

	/* read in one block to find the FIT size */
	blknum =  offset / desc->blksz;
	log_debug("read at %lx, blknum %lx\n", offset, blknum);
	ret = blk_read(blk, blknum, 1, &sbuf);
	if (ret < 0)
		return log_msg_ret("rd", ret);

	ret = fdt_check_header(&sbuf);
	if (ret < 0)
		return log_msg_ret("fdt", -EINVAL);
	size = fdt_totalsize(&sbuf);
	if (size > priv->area_size)
		return log_msg_ret("fdt", -E2BIG);
	bflow->size = size;
	log_debug("FIT size %lx\n", size);

	addr = CONFIG_SPL_TEXT_BASE;
	buf = map_sysmem(addr, size);
	num_blks = size / desc->blksz;
	log_debug("read %lx, %lx blocks to %p\n", size, num_blks, buf);
	ret = blk_read(blk, blknum, num_blks, buf);
	if (ret < 0)
		return log_msg_ret("rd", ret);

	fit_uname = NULL;
	fit_uname_config = NULL;
	log_debug("loading FIT\n");
	ret = fit_image_load(&images, addr, &fit_uname, &fit_uname_config,
			     IH_ARCH_SANDBOX, IH_TYPE_SPL_FIRMWARE,
			     BOOTSTAGE_ID_FIT_SPL_START, FIT_LOAD_IGNORED,
			     &load_addr, &len);


	return 0;
}

static struct bootmeth_ops bootmeth_vbe_simple_ops = {
	.get_state_desc	= vbe_simple_get_state_desc,
	.read_bootflow	= vbe_simple_read_bootflow,
	.read_file	= vbe_simple_read_file,
};

int vbe_simple_fixup_node(ofnode node, struct simple_state *state)
{
	char *version;
	int ret;

	version = strdup(state->fw_version);
	if (!version)
		return log_msg_ret("dup", -ENOMEM);

	ret = ofnode_write_string(node, "cur-version", version);
	if (ret)
		return log_msg_ret("ver", ret);
	ret = ofnode_write_u32(node, "cur-vernum", state->fw_vernum);
	if (ret)
		return log_msg_ret("num", ret);

	/* For SPL the version is added once we get to U-Boot proper */
	if (!IS_ENABLED(CONFIG_SPL_BUILD)) {
		ret = ofnode_write_string(node, "bootloader-version",
					  version_string);
		if (ret)
			return log_msg_ret("bl", ret);
	}

	return 0;
}

/**
 * bootmeth_vbe_simple_ft_fixup() - Write out all VBE simple data to the DT
 *
 * @ctx: Context for event
 * @event: Event to process
 * @return 0 if OK, -ve on error
 */
static int bootmeth_vbe_simple_ft_fixup(void *ctx, struct event *event)
{
	oftree tree = event->data.ft_fixup.tree;
	struct udevice *dev;

	/*
	 * Ideally we would have driver model support for fixups, but that does
	 * not exist yet. It is a step too far to try to do this before VBE is
	 * in place.
	 */
	for (vbe_find_first_device(&dev); dev; vbe_find_next_device(&dev)) {
		struct simple_state state;
		ofnode node, subnode;
		int ret;

		if (strcmp("vbe_simple", dev->driver->name))
			continue;

		/* Check if there is a node to fix up */
		node = oftree_path(tree, "/chosen/fwupd");
		if (!ofnode_valid(node))
			continue;
		subnode = ofnode_find_subnode(node, dev->name);
		if (!ofnode_valid(subnode))
			continue;

		log_debug("Fixing up: %s\n", dev->name);
		ret = device_probe(dev);
		if (ret)
			return log_msg_ret("probe", ret);
		ret = simple_read_state(dev, &state);
		if (ret)
			return log_msg_ret("read", ret);

		ret = vbe_simple_fixup_node(subnode, &state);
		if (ret)
			return log_msg_ret("fix", ret);
	}

	return 0;
}
EVENT_SPY(EVT_FT_FIXUP, bootmeth_vbe_simple_ft_fixup);

static int bootmeth_vbe_simple_probe(struct udevice *dev)
{
	struct simple_priv *priv = dev_get_priv(dev);

	memset(priv, '\0', sizeof(*priv));
	if (dev_read_u32(dev, "area-start", &priv->area_start) ||
	    dev_read_u32(dev, "area-size", &priv->area_size) ||
	    dev_read_u32(dev, "version-offset", &priv->version_offset) ||
	    dev_read_u32(dev, "version-size", &priv->version_size) ||
	    dev_read_u32(dev, "state-offset", &priv->state_offset) ||
	    dev_read_u32(dev, "state-size", &priv->state_size))
		return log_msg_ret("read", -EINVAL);
	dev_read_u32(dev, "skip-offset", &priv->skip_offset);
	priv->storage = strdup(dev_read_string(dev, "storage"));
	if (!priv->storage)
		return log_msg_ret("str", -EINVAL);

	return 0;
}

static int bootmeth_vbe_simple_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = IS_ENABLED(CONFIG_BOOTSTD_FULL) ?
		"VBE simple" : "vbe-simple";
	plat->flags = BOOTMETHF_GLOBAL;

	return 0;
}

static int simple_load_from_image(struct spl_image_info *spl_image,
				  struct spl_boot_device *bootdev)
{
	struct udevice *vdev, *bdev;
	struct simple_priv *priv;
	struct bootflow bflow;
	int ret;

	if (!IS_ENABLED(CONFIG_VPL_BUILD))
		return -ENOENT;

	vbe_find_first_device(&vdev);
	if (!vdev)
		return log_msg_ret("vd", -ENODEV);
	log_debug("vbe dev %s\n", vdev->name);
	ret = device_probe(vdev);
	if (ret)
		return log_msg_ret("probe", ret);

	priv = dev_get_priv(vdev);
	log_debug("simple %s\n", priv->storage);
	ret = bootdev_find_by_label(priv->storage, &bdev);
	if (ret)
		return log_msg_ret("bd", ret);
	log_debug("bootdev %s\n", bdev->name);

	ret = vbe_simple_read_fw_bootflow(bdev, vdev, &bflow);
	log_debug("fw ret=%d\n", ret);
	if (ret)
		return log_msg_ret("rd", ret);

	/* To be implemented */

	return -ENOENT;

	return 0;
}
SPL_LOAD_IMAGE_METHOD("vbe_simple", 5, BOOT_DEVICE_VBE,
		      simple_load_from_image);

#if CONFIG_IS_ENABLED(OF_REAL)
static const struct udevice_id generic_simple_vbe_simple_ids[] = {
	{ .compatible = "fwupd,vbe-simple" },
	{ }
};
#endif

U_BOOT_DRIVER(vbe_simple) = {
	.name	= "vbe_simple",
	.id	= UCLASS_BOOTMETH,
	.of_match = of_match_ptr(generic_simple_vbe_simple_ids),
	.ops	= &bootmeth_vbe_simple_ops,
	.bind	= bootmeth_vbe_simple_bind,
	.probe	= bootmeth_vbe_simple_probe,
	.flags	= DM_FLAG_PRE_RELOC,
	.priv_auto	= sizeof(struct simple_priv),
};
