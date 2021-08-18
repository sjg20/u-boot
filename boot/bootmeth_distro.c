// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for distro boot (syslinux boot from a block device)
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <command.h>
#include <distro.h>
#include <dm.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>
#include <mmc.h>
#include <pxe_utils.h>

static int disto_getfile(struct pxe_context *ctx, const char *file_path,
			 char *file_addr, ulong *sizep)
{
	struct distro_info *info = ctx->userdata;
	ulong addr;
	int ret;

	addr = simple_strtoul(file_addr, NULL, 16);

	/* Allow up to 1GB */
	*sizep = 1 << 30;
	ret = bootmeth_read_file(info->dev, info->bflow, file_path, addr,
				 sizep);
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

static int distro_check(struct udevice *dev, struct bootflow_iter *iter)
{
	int ret;

	/* This only works on block devices */
	ret = bootflow_iter_uses_blk_dev(iter);
	if (ret)
		return log_msg_ret("blk", ret);

	return 0;
}

static int distro_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	const char *const *prefixes;
	struct udevice *bootstd;
	loff_t size, bytes_read;
	char fname[200];
	ulong addr;
	int ret, i;
	char *buf;

	ret = uclass_first_device_err(UCLASS_BOOTSTD, &bootstd);
	if (ret)
		return log_msg_ret("std", ret);

	/* We require a partition table */
	if (!bflow->part)
		return -ENOENT;

	bflow->fname = strdup(DISTRO_FNAME);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);

	prefixes = bootstd_get_prefixes(bootstd);
	if (prefixes) {
		log_debug("Trying prefixes:\n");
		for (i = 0; prefixes[i]; i++) {
			snprintf(fname, sizeof(fname), "%s%s", prefixes[i],
				 DISTRO_FNAME);
			ret = fs_size(fname, &size);
			log_debug("   %s - err=%d\n", fname, ret);
			if (!ret)
				break;

			/*
			 * Sadly FS closes the file after fs_size() so we must
			 * redo this
			 */
			ret = fs_set_blk_dev_with_part(desc, bflow->part);
			if (ret)
				return log_msg_ret("set", ret);
		}
		log_debug("   done\n");
	} else {
		strcpy(fname, DISTRO_FNAME);
		ret = fs_size(bflow->fname, &size);
		log_debug("No prefixes: %s - err=%d", fname, ret);
	}
	if (ret)
		return log_msg_ret("size", ret);

	bflow->fname = strdup(fname);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_FILE;
	bflow->size = size;
	log_debug("   - distro file size %x\n", (uint)size);
	if (size > 0x10000)
		return log_msg_ret("chk", -E2BIG);

	/* Sadly FS closes the file after fs_size() so we must redo this */
	ret = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret)
		return log_msg_ret("set", ret);

	buf = malloc(size + 1);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	addr = map_to_sysmem(buf);

	ret = fs_read(bflow->fname, addr, 0, 0, &bytes_read);
	if (ret) {
		free(buf);
		return log_msg_ret("read", ret);
	}
	if (size != bytes_read)
		return log_msg_ret("bread", -EINVAL);
	buf[size] = '\0';
	bflow->state = BOOTFLOWST_READY;
	bflow->buf = buf;

	return 0;
}

static int distro_read_file(struct udevice *dev, struct bootflow *bflow,
			    const char *file_path, ulong addr, ulong *sizep)
{
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	loff_t len_read;
	loff_t size;
	int ret;

	ret = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret)
		return log_msg_ret("set1", ret);
	ret = fs_size(file_path, &size);
	if (ret)
		return log_msg_ret("size", ret);
	if (size > *sizep)
		return log_msg_ret("spc", -ENOSPC);

	ret = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret)
		return log_msg_ret("set2", ret);
	ret = fs_read(file_path, addr, 0, 0, &len_read);
	if (ret)
		return ret;
	*sizep = len_read;

	return 0;
}

static int distro_boot(struct udevice *dev, struct bootflow *bflow)
{
	struct cmd_tbl cmdtp = {};	/* dummy */
	struct pxe_context ctx;
	struct distro_info info;
	ulong addr;
	int ret;

	addr = map_to_sysmem(bflow->buf);
	info.dev = dev;
	info.bflow = bflow;
	ret = pxe_setup_ctx(&ctx, &cmdtp, disto_getfile, &info, true,
			    bflow->subdir);
	if (ret)
		return log_msg_ret("ctx", -EINVAL);

	ret = pxe_process(&ctx, addr, false);
	if (ret)
		return log_msg_ret("bread", -EINVAL);

	return 0;
}

static int distro_bootmeth_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = "Syslinux boot from a block device";

	return 0;
}

static struct bootmeth_ops distro_bootmeth_ops = {
	.check		= distro_check,
	.read_bootflow	= distro_read_bootflow,
	.read_file	= distro_read_file,
	.boot		= distro_boot,
};

static const struct udevice_id distro_bootmeth_ids[] = {
	{ .compatible = "u-boot,distro-syslinux" },
	{ }
};

U_BOOT_DRIVER(bootmeth_distro) = {
	.name		= "bootmeth_distro",
	.id		= UCLASS_BOOTMETH,
	.of_match	= distro_bootmeth_ids,
	.ops		= &distro_bootmeth_ops,
	.bind		= distro_bootmeth_bind,
};
