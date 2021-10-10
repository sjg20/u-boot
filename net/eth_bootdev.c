// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootdevice for ethernet (uses PXE)
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <command.h>
#include <bootmeth.h>
#include <distro.h>
#include <dm.h>
#include <net.h>

static int eth_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			    struct bootflow *bflow)
{
	struct udevice *media_dev = dev_get_parent(dev);
	char name[60];
	int ret;

	/* Must be an Ethernet device */
	if (device_get_uclass_id(media_dev) != UCLASS_ETH)
		return -ENOENT;

	/*
	 * Like distro boot, this assumes there is only one Ethernet device.
	 * In this case, that means that @eth is ignored
	 */

	snprintf(name, sizeof(name), "%s.%d", dev->name, iter->part);
	bflow->name = strdup(name);
	if (!bflow->name)
		return log_msg_ret("name", -ENOMEM);
	bflow->state = BOOTFLOWST_BASE;

	/*
	 * There is not a direct interface to the network stack so run
	 * everything through the command-line interpreter for now.
	 *
	 * Don't bother checking the result of dhcp. It can fail with:
	 *
	 * DHCP client bound to address 192.168.4.50 (4 ms)
	 * *** Warning: no boot file name; using 'C0A80432.img'
	 * Using smsc95xx_eth device
	 * TFTP from server 192.168.4.1; our IP address is 192.168.4.50
	 * Filename 'C0A80432.img'.
	 * Load address: 0x200000
	 * Loading: *
	 * TFTP error: 'File not found' (1)
	 *
	 * This is not a real failure, since we don't actually care if the
	 * boot file exists.
	 */
	run_command("dhcp", 0);
	bflow->state = BOOTFLOWST_MEDIA;

	ret = bootmeth_read_bootflow(bflow->method, bflow);
	if (ret)
		return log_msg_ret("method", ret);

	return 0;
}

static int eth_bootdev_bind(struct udevice *dev)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->prio = BOOTDEVP_4_NET_BASE;

	return 0;
}

struct bootdev_ops eth_bootdev_ops = {
	.get_bootflow	= eth_get_bootflow,
};

U_BOOT_DRIVER(eth_bootdev) = {
	.name		= "eth_bootdev",
	.id		= UCLASS_BOOTDEV,
	.ops		= &eth_bootdev_ops,
	.bind		= eth_bootdev_bind,
};
