// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootmethod' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <command.h>
#include <dm.h>
#include <dm/uclass-internal.h>

static int bootmethod_check_state(struct bootflow_state **statep)
{
	struct bootflow_state *state;
	int ret;

	ret = bootmethod_get_state(&state);
	if (ret)
		return ret;
	if (!state->cur_bootmethod) {
		printf("Please use 'bootmethod select' first\n");
		return -ENOENT;
	}
	*statep = state;

	return 0;
}

static int do_bootmethod_list(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	bool probe;

	probe = argc >= 2 && !strcmp(argv[1], "-p");
	bootmethod_list(probe);

	return 0;
}

static int do_bootmethod_select(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	struct bootflow_state *state;
	struct udevice *dev;
	const char *name;
	char *endp;
	int seq;
	int ret;

	ret = bootmethod_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
	if (argc < 2) {
		state->cur_bootmethod = NULL;
		return 0;
	}
	name = argv[1];
	seq = simple_strtol(name, &endp, 16);
	if (*endp)
		ret = uclass_get_device_by_name(UCLASS_BOOTMETHOD, name, &dev);
	else
		ret = uclass_get_device_by_seq(UCLASS_BOOTMETHOD, seq, &dev);
	if (ret) {
		printf("Cannot find '%s' (err=%d)\n", name, ret);
		return CMD_RET_FAILURE;
	}
	state->cur_bootmethod = dev;

	return 0;
}

static int do_bootmethod_info(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	struct bootflow_state *state;
	int ret;

	ret = bootmethod_check_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
	printf("%s\n", state->cur_bootmethod->name);

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootmethod_help_text[] =
	"list [-p]      - list all available bootmethods (-p to probe)\n"
	"bootmethod select <bm>    - select a bootmethod by name\n"
	"bootmethod info           - show information about a bootmethod";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootmethod, "Bootmethods", bootmethod_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootmethod_list),
	U_BOOT_SUBCMD_MKENT(select, 2, 1, do_bootmethod_select),
	U_BOOT_SUBCMD_MKENT(info, 1, 1, do_bootmethod_info));
