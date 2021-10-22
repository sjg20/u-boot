// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for EFI boot manager
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

static int efi_mgr_check(struct udevice *dev, struct bootflow_iter *iter)
{
	int ret;

	if (iter->flags & BOOTFLOWF_EFI_BOOTMGR_DONE)
		return -ENOTSUPP;

	/*
	 * Only allow this on block devices, just to limit the number of times
	 * it is tried. In fact, it scans all devices and is a law unto itself.
	 */
	ret = bootflow_iter_uses_blk_dev(iter);
	if (ret)
		return log_msg_ret("blk", ret);

	iter->flags |= BOOTFLOWF_EFI_BOOTMGR_DONE;

	return 0;
}

static int efi_mgr_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	/*
	 * Just assume there is something to boot since we don't have any way
	 * of knowing in advance
	 */
	bflow->state = BOOTFLOWST_READY;

	return 0;
}

static int efi_mgr_read_file(struct udevice *dev, struct bootflow *bflow,
				const char *file_path, ulong addr, ulong *sizep)
{
	/* Files are loaded by the 'bootefi bootmgr' command */

	return -ENOSYS;
}

static int efi_mgr_boot(struct udevice *dev, struct bootflow *bflow)
{
	int ret;

	/* Booting is handled by the 'bootefi bootmgr' command */
	ret = run_command("bootefi bootmgr", 0);

	/*
	 * If this returns then the boot failed. ALl available options were
	 * presumably tried so there is no point in using this bootmeth again
	 */
	log_warning("EFI bootmgr did not boot: disabling this boot method\n");

	return -ENOTSUPP;
}

static int bootmeth_efi_mgr_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = "EFI bootmgr flow";

	return 0;
}

static struct bootmeth_ops efi_mgr_bootmeth_ops = {
	.check		= efi_mgr_check,
	.read_bootflow	= efi_mgr_read_bootflow,
	.read_file	= efi_mgr_read_file,
	.boot		= efi_mgr_boot,
};

static const struct udevice_id efi_mgr_bootmeth_ids[] = {
	{ .compatible = "u-boot,efi-bootmgr" },
	{ }
};

/* Name this so it comes last */
U_BOOT_DRIVER(bootmeth_zefi_mgr) = {
	.name		= "bootmeth_zefi_mgr",
	.id		= UCLASS_BOOTMETH,
	.of_match	= efi_mgr_bootmeth_ids,
	.ops		= &efi_mgr_bootmeth_ops,
	.bind		= bootmeth_efi_mgr_bind,
};
