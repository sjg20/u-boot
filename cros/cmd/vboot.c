// SPDX-License-Identifier: GPL-2.0+
/*
 * Implements the 'vboot' command which provides access to the verified boot
 * flow.
 *
 * TODO(sjg@chromium.org): Add a test for this command
 *
 * Copyright 2018 Google LLC
 */

#define NEED_VB20_INTERNALS

#include <common.h>
#include <command.h>
#include <dm.h>
#include <ec_commands.h>
#include <cros/nvdata.h>
#include <cros/stages.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>

/* The next stage of vboot to run (used for repeatable commands) */
static enum vboot_stage_t vboot_next_stage;

int board_run_command(const char *cmd)
{
	struct vboot_info *vboot = vboot_get_alloc();

	printf("Secure boot mode: %s\n", cmd);
	if (!strcmp(cmd, "vboot") || !strcmp(cmd, "vboot_go_auto")) {
		vboot_run_auto(vboot, 0);
		/* Should not return */
	} else {
		printf("Unknown command '%s'\n", cmd);
		panic("board_run_command() failed");
	}

	return CMD_RET_FAILURE;
}

static int do_vboot_go(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct vboot_info *vboot = vboot_get_alloc();
	const char *stage;
	uint flags = 0;
	int ret;

	/* strip off 'go' */
	argc--;
	argv++;
	if (argc < 1)
		return CMD_RET_USAGE;
	if (!strcmp("-n", argv[0])) {
		flags |= VBOOT_FLAG_CMDLINE;
		argc--;
		argv++;
		if (argc < 1)
			return CMD_RET_USAGE;
	}

	stage = argv[0];
	if (!strcmp(stage, "ro")) {
		ret = vboot_run_stages(vboot, VBOOT_STAGE_FIRST, flags);
	} else if (!strcmp(stage, "rw")) {
		ret = vboot_run_stages(vboot, VBOOT_STAGE_RW_FIRST_SPL, flags);
	} else if (!strcmp(stage, "auto")) {
		ret = vboot_run_auto(vboot, flags);
	} else {
		enum vboot_stage_t stagenum;

		if (flag & CMD_FLAG_REPEAT) {
			stagenum = vboot_next_stage;
		} else {
			if (!strcmp("start", stage)) {
				stagenum = VBOOT_STAGE_FIRST;
			} else if (!strcmp("start_rw", stage)) {
				stagenum = VBOOT_STAGE_RW_FIRST_SPL;
			} else if (!strcmp("next", stage)) {
				stagenum = vboot_next_stage;
			} else {
				stagenum = vboot_find_stage(stage);
				if (stagenum == VBOOT_STAGE_NONE) {
					printf("Umknown stage\n");
					return CMD_RET_USAGE;
				}
			}
		}
		if (stagenum == VBOOT_STAGE_COUNT) {
			printf("All vboot stages are complete\n");
			return 1;
		}

		ret = vboot_run_stage(vboot, stagenum);
		if (!ret)
			vboot_next_stage = stagenum + 1;
	}

	return ret ? CMD_RET_FAILURE : 0;
}

static int do_vboot_list(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	enum vboot_stage_t stagenum;
	const char *name;

	printf("Available stages:\n");
	for (stagenum = VBOOT_STAGE_FIRST_VER; stagenum < VBOOT_STAGE_COUNT;
	     stagenum++) {
		name = vboot_get_stage_name(stagenum);
		printf("   %d: %s\n", stagenum, name);
	}

	return 0;
}

static int do_vboot_flags(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{

	int i;

	for (i = 0; i < VBOOT_FLAG_COUNT; i++) {
		struct udevice *dev;
		int prev;
		int val;

		val = vboot_flag_read_walk_prev(i, &prev, &dev);

		printf("%-15s: %-18s: value=%d, prev=%d\n", vboot_flag_name(i),
		       dev ? dev->driver->name: "(none)", val, prev);
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char vboot_help_text[] =
	 "go -n [ro|rw|auto|start|next|<stage>]  Run verified boot stage (repeatable)\n"
	 "       -n = drop to cmdline on failure\n"
	 "vboot list           List verified boot stages\n"
	 "vboot flags          Show values of flags";
#endif

U_BOOT_CMD_WITH_SUBCMDS(vboot, "Chromium OS Verified boot", vboot_help_text,
	U_BOOT_CMD_MKENT(go, 4, 0, do_vboot_go, "", ""),
	U_BOOT_CMD_MKENT(list, 4, 0, do_vboot_list, "", ""),
	U_BOOT_CMD_MKENT(flags, 4, 0, do_vboot_flags, "", ""),
);

static int dump_nvdata(void)
{
	u8 nvdata[EC_VBNV_BLOCK_SIZE];
	int ret;

	ret = cros_nvdata_read_walk(CROS_NV_DATA, nvdata, sizeof(nvdata));
	if (ret)
		return log_msg_ret("read", ret);
	ret = vboot_dump_nvdata(nvdata, sizeof(nvdata));
	if (ret)
		return log_msg_ret("dump", ret);

	return 0;
}

static int do_nvdata_dump(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	int ret;

	ret = dump_nvdata();
	if (ret) {
		printf("Error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char nvdata_help_text[] =
	"dump     Dump non-volatile vboot data";
#endif

U_BOOT_CMD_WITH_SUBCMDS(nvdata, "Non-volatile data", nvdata_help_text,
	U_BOOT_CMD_MKENT(dump, 1, 0, do_nvdata_dump, "", "")
);

static int dump_secdata(void)
{
	u8 secdata[sizeof(struct vb2_secdata)];
	int ret;

	ret = cros_nvdata_read_walk(CROS_NV_SECDATA, secdata, sizeof(secdata));
	if (ret)
		return log_msg_ret("read", ret);
	ret = vboot_secdata_dump(secdata, sizeof(secdata));
	if (ret)
		return log_msg_ret("dump", ret);

	return 0;
}

static int do_secdata_dump(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	int ret;

	ret = dump_secdata();
	if (ret) {
		printf("Error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

const char *const secdata_name[] = {
	[SECDATA_DEV_MODE] = "dev_mode",
	[SECDATA_LAST_BOOT_DEV] = "last_boot_dev",
};

static int do_secdata_set(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	u8 secdata[sizeof(struct vb2_secdata)];
	int ret, i;

	ret = cros_nvdata_read_walk(CROS_NV_SECDATA, secdata, sizeof(secdata));
	if (ret) {
		printf("Cannot read (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	if (argc <= 1) {
		for (i = 0; i < SECDATA_COUNT; i++) {
			int val = vboot_secdata_get(secdata, sizeof(secdata),
						    i);

			printf("%s: %d (%#x)\n", secdata_name[i], val, val);
		}

	} else if (argc == 3) {
		enum secdata_t field = SECDATA_NONE;
		int val;

		for (i = 0; i < SECDATA_COUNT; i++) {
			if (!strcmp(argv[1], secdata_name[i])) {
				field = i;
				break;
			}
		}
		if (field == SECDATA_NONE) {
			printf("Unknown field '%s'\n", argv[1]);
			return CMD_RET_USAGE;
		}

		val = simple_strtol(argv[2], NULL, 16);
		printf("Set '%s' to %x\n", secdata_name[field], val);
		ret = vboot_secdata_set(secdata, sizeof(secdata), field, val);
		if (ret) {
			printf("Cannot set (err=%d)\n", ret);
			return CMD_RET_FAILURE;
		}
		ret = cros_nvdata_write_walk(CROS_NV_SECDATA, secdata,
					     sizeof(secdata));
		if (ret) {
			printf("Cannot write (err=%d)\n", ret);
			return CMD_RET_FAILURE;
		}
	} else {
		return CMD_RET_USAGE;
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char secdata_help_text[] =
	"dump     Dump secure vboot data\n"
	"secdata set      Set/Get secure vboot data";
#endif

U_BOOT_CMD_WITH_SUBCMDS(secdata, "Cros vboot boot secure data",
			secdata_help_text,
	U_BOOT_CMD_MKENT(dump, 4, 0, do_secdata_dump, "", ""),
	U_BOOT_CMD_MKENT(set, 4, 0, do_secdata_set, "", ""),
);

static int do_vboot_go_auto(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	board_run_command("vboot");

	return 0;
}

U_BOOT_CMD(vboot_go_auto, 4, 1, do_vboot_go_auto, "Chromium OS Verified boot",
	   "      Run full verified boot");
