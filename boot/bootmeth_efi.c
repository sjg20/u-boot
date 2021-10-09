// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for distro boot via EFI
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <command.h>
#include <dm.h>
#include <efi_loader.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>
#include <mmc.h>
#include <pxe_utils.h>

#define EFI_DIRNAME	"efi/boot/"

/**
 * get_efi_leafname() - Get the leaf name for the EFI file we expect
 *
 * @str: Place to put leaf name for this architecture, e.g. "bootaa64.efi".
 *	Must have at least 16 bytes of space
 * @max_len: Length of @str, must be >=16
 */
static int get_efi_leafname(char *str, int max_len)
{
	const char *base;

	if (max_len < 16)
		return log_msg_ret("spc", -ENOSPC);
	if (IS_ENABLED(CONFIG_ARM64))
		base = "bootaa64";
	else if (IS_ENABLED(CONFIG_ARM))
		base = "bootarm";
	else if (IS_ENABLED(CONFIG_X86_RUN_32BIT))
		base = "bootia32";
	else if (IS_ENABLED(CONFIG_X86_RUN_64BIT))
		base = "bootx64";
	else if (IS_ENABLED(CONFIG_ARCH_RV32I))
		base = "bootriscv32";
	else if (IS_ENABLED(CONFIG_ARCH_RV64I))
		base = "bootriscv64";
	else if (IS_ENABLED(CONFIG_SANDBOX))
		base = "bootsbox";
	else
		return -EINVAL;

	strcpy(str, base);
	strcat(str, ".efi");

	return 0;
}

static int efiload_read_file(struct blk_desc *desc, struct bootflow *bflow)
{
	const struct udevice *media_dev;
	int size = bflow->size;
	char devnum_str[9];
	char dirname[200];
	loff_t bytes_read;
	char *last_slash;
	ulong addr;
	char *buf;
	int ret;

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

	/*
	 * This is a horrible hack to tell EFI about this boot device. Once we
	 * unify EFI with the rest of U-Boot we can clean this up. The same hack
	 * exists in multiple places, e.g. in the fs, tftp and load commands.
	 *
	 * Once we can clean up the EFI code to make proper use of driver model,
	 * this can go away.
	 */
	media_dev = dev_get_parent(bflow->dev);
	snprintf(devnum_str, sizeof(devnum_str), "%x", dev_seq(media_dev));

	strlcpy(dirname, bflow->fname, sizeof(dirname));
	last_slash = strrchr(dirname, '/');
	if (last_slash)
		*last_slash = '\0';

	efi_set_bootdev(dev_get_uclass_name(media_dev), devnum_str, dirname,
			bflow->buf, size);

	return 0;
}

static int distro_efi_check(struct udevice *dev, struct bootflow_iter *iter)
{
	int ret;

	/* This only works on block devices */
	ret = bootflow_iter_uses_blk_dev(iter);
	if (ret)
		return log_msg_ret("blk", ret);

	return 0;
}

static int distro_efi_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	char fname[sizeof(EFI_DIRNAME) + 16];
	loff_t size;
	int ret;

	/* We require a partition table */
	if (!bflow->part)
		return -ENOENT;

	strcpy(fname, EFI_DIRNAME);
	ret = get_efi_leafname(fname + strlen(fname),
			       sizeof(fname) - strlen(fname));
	if (ret)
		return log_msg_ret("leaf", ret);

	bflow->fname = strdup(fname);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);
	ret = fs_size(bflow->fname, &size);
	bflow->size = size;
	if (ret)
		return log_msg_ret("size", ret);
	bflow->state = BOOTFLOWST_FILE;
	log_debug("   - distro file size %x\n", (uint)size);
	if (size > 0x2000000)
		return log_msg_ret("chk", -E2BIG);

	ret = efiload_read_file(desc, bflow);
	if (ret)
		return log_msg_ret("read", -EINVAL);

	return 0;
}

static int distro_efi_read_file(struct udevice *dev, struct bootflow *bflow,
				const char *file_path, ulong addr, ulong *sizep)
{
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	const struct udevice *media_dev;
	int size = bflow->size;
	char devnum_str[9];
	char dirname[200];
	loff_t bytes_read;
	char *last_slash;
	char *buf;
	int ret;

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

	/*
	 * This is a horrible hack to tell EFI about this boot device. Once we
	 * unify EFI with the rest of U-Boot we can clean this up. The same hack
	 * exists in multiple places, e.g. in the fs, tftp and load commands.
	 *
	 * Once we can clean up the EFI code to make proper use of driver model,
	 * this can go away.
	 */
	media_dev = dev_get_parent(bflow->dev);
	snprintf(devnum_str, sizeof(devnum_str), "%x", dev_seq(media_dev));

	strlcpy(dirname, bflow->fname, sizeof(dirname));
	last_slash = strrchr(dirname, '/');
	if (last_slash)
		*last_slash = '\0';

	efi_set_bootdev(dev_get_uclass_name(media_dev), devnum_str, dirname,
			bflow->buf, size);

	return 0;
}

int distro_efi_boot(struct udevice *dev, struct bootflow *bflow)
{
	char cmd[50];

	/*
	 * At some point we can add a real interface to bootefi so we can call
	 * this directly. For now, go through the CLI like distro boot.
	 */
	snprintf(cmd, sizeof(cmd), "bootefi %lx %lx",
		 (ulong)map_to_sysmem(bflow->buf),
		 (ulong)map_to_sysmem(gd->fdt_blob));
	if (run_command(cmd, 0))
		return log_msg_ret("run", -EINVAL);

	return 0;
}

static int distro_bootmeth_efi_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = "EFI boot from a .efi file";

	return 0;
}

static struct bootmeth_ops distro_efi_bootmeth_ops = {
	.check		= distro_efi_check,
	.read_bootflow	= distro_efi_read_bootflow,
	.read_file	= distro_efi_read_file,
	.boot		= distro_efi_boot,
};

static const struct udevice_id distro_efi_bootmeth_ids[] = {
	{ .compatible = "u-boot,distro-efi" },
	{ }
};

U_BOOT_DRIVER(bootmeth_efi) = {
	.name		= "bootmeth_efi",
	.id		= UCLASS_BOOTMETH,
	.of_match	= distro_efi_bootmeth_ids,
	.ops		= &distro_efi_bootmeth_ops,
	.bind		= distro_bootmeth_efi_bind,
};
