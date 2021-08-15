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
#include <dm.h>
#include <net.h>

static int eth_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	ulong addr;
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
	 */
	ret = run_command("dhcp", 0);
	if (ret)
		return log_msg_ret("dhcp", -EIO);
	bflow->state = BOOTFLOWST_MEDIA;

	if (CONFIG_IS_ENABLED(BOOTMETHOD_DISTRO)) {
		ret = distro_net_setup();
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
