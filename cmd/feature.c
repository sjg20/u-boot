// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <feature.h>
#include <dm/device-internal.h>

#define DTIME_MS	20

static int get_uclass(struct uclass **ucp)
{
	int ret;

	ret = uclass_get(UCLASS_FEATURE, ucp);
	if (ret) {
		printf("Failed to find uclass (err=%d)\n", ret);
		return ret;
	}

	return 0;
}

static int find_feature(const char *name, struct udevice **devp)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	*devp = NULL;
	ret = get_uclass(&uc);
	if (!uc)
		return ret;
	uclass_foreach_dev(dev, uc) {
		if (!strcmp(name, dev->name)) {
			ret = device_probe(dev);
			if (ret)
				return log_msg_ret("probe", ret);
			*devp = dev;
			return 0;
		}
	}

	return -ENODEV;
}

int feature_run(struct udevice *dev)
{

	return 0;
}

static int do_feature_run(cmd_tbl_t *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct udevice *dev;
	const char *name;
	bool running;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;
	name = argv[1];
	ret = find_feature(name, &dev);
	if (ret) {
		printf("Feature '%s' not found (err=%d)\n", name, ret);
		return CMD_RET_FAILURE;
	}

	for (running = true; running;) {
		ulong started;

		started = get_timer(0);
		ret = feature_poll(dev);
		if (ret)
			return log_msg_ret("poll", ret);
		if (ctrlc())
			running = false;
		while (get_timer(started) < DTIME_MS)
			;
	}

	return 0;
}

static int do_feature_list(cmd_tbl_t *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = get_uclass(&uc);
	if (ret)
		return CMD_RET_FAILURE;
	printf("Features:\n");
	uclass_foreach_dev(dev, uc)
		printf("   %s\n", dev->name);

	return 0;
}

static char feature_help_text[] =
	"list - list features\n"
	"feature run <name> - Run a feature in a loop for testing";

U_BOOT_CMD_WITH_SUBCMDS(feature, "U-Boot features", feature_help_text,
	U_BOOT_SUBCMD_MKENT(list, 1, 1, do_feature_list),
	U_BOOT_SUBCMD_MKENT(run, 2, 1, do_feature_run));
