// /* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Bootmethod for ethernet
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <command.h>
#include <distro.h>
#include <dm.h>
#include <net.h>

static int eth_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	int ret;

	/*
	 * Like distro boot, this assumes there is only one Ethernet device.
	 * In this case, that means that @eth is ignored
	 */
	if (seq)
		return log_msg_ret("dhcp", -ESHUTDOWN);

	bflow->seq = seq;
	bflow->name = strdup(dev->name);
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

	if (CONFIG_IS_ENABLED(BOOTMETHOD_DISTRO)) {
		ret = distro_net_setup(bflow);
		printf("ret=%d\n", ret);
		if (ret)
			return log_msg_ret("distro", ret);
	}

	return 0;
}

struct bootmethod_ops eth_bootmethod_ops = {
	.get_bootflow	= eth_get_bootflow,
};

U_BOOT_DRIVER(eth_bootmethod) = {
	.name		= "eth_bootmethod",
	.id		= UCLASS_BOOTMETHOD,
	.ops		= &eth_bootmethod_ops,
};
