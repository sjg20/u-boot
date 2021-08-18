// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootdev' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <command.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>

static int bootdev_check_state(struct bootdev_state **statep)
{
	struct bootdev_state *state;
	int ret;

	ret = bootdev_get_state(&state);
	if (ret)
		return ret;
	if (!state->cur_bootdev) {
		printf("Please use 'bootdev select' first\n");
		return -ENOENT;
	}
	*statep = state;

	return 0;
}

static int do_bootdev_list(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	bool probe;

	probe = argc >= 2 && !strcmp(argv[1], "-p");
	bootdev_list(probe);

	return 0;
}

static int do_bootdev_select(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	struct bootdev_state *state;
	struct udevice *dev;
	const char *name;
	char *endp;
	int seq;
	int ret;

	ret = bootdev_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
	if (argc < 2) {
		state->cur_bootdev = NULL;
		return 0;
	}
	name = argv[1];
	seq = simple_strtol(name, &endp, 16);

	/* Select by name or number */
	if (*endp)
		ret = uclass_get_device_by_name(UCLASS_BOOTDEV, name, &dev);
	else
		ret = uclass_get_device_by_seq(UCLASS_BOOTDEV, seq, &dev);
	if (ret) {
		printf("Cannot find '%s' (err=%d)\n", name, ret);
		return CMD_RET_FAILURE;
	}
	state->cur_bootdev = dev;

	return 0;
}

static int do_bootdev_info(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct bootdev_state *state;
	struct bootflow *bflow;
	int ret, i, num_valid;
	struct udevice *dev;
	bool probe;

	probe = argc >= 2 && !strcmp(argv[1], "-p");

	ret = bootdev_check_state(&state);
	if (ret)
		return CMD_RET_FAILURE;

	dev = state->cur_bootdev;

	/* Count the number of bootflows, including how many are valid*/
	num_valid = 0;
	for (ret = bootdev_first_bootflow(dev, &bflow), i = 0;
	     !ret;
	     ret = bootdev_next_bootflow(&bflow), i++)
		num_valid += bflow->state == BOOTFLOWST_LOADED;

	/*
	 * Prove the device, if requested, otherwise assume that there is no
	 * error
	 */
	ret = 0;
	if (probe)
		ret = device_probe(dev);

	printf("Name:      %s\n", dev->name);
	printf("Sequence:  %d\n", dev_seq(dev));
	printf("Status:    %s\n", ret ? simple_itoa(ret) : device_active(dev) ?
		"Probed" : "OK");
	printf("Uclass:    %s\n", dev_get_uclass_name(dev_get_parent(dev)));
	printf("Bootflows: %d (%d valid)\n", i, num_valid);

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootdev_help_text[] =
	"list [-p]      - list all available bootdevs (-p to probe)\n"
	"bootdev select <bm>    - select a bootdev by name\n"
	"bootdev info [-p]      - show information about a bootdev (-p to probe)";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootdev, "Bootdevices", bootdev_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootdev_list),
	U_BOOT_SUBCMD_MKENT(select, 2, 1, do_bootdev_select),
	U_BOOT_SUBCMD_MKENT(info, 2, 1, do_bootdev_info));
