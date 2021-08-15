// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootflow' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <mapmem.h>

/**
 * report_bootflow_err() - Report where a bootflow failed
 *
 * When a bootflow does not make it to the 'loaded' state, something went wrong.
 * Print a helpful message if there is an error
 *
 * @bflow: Bootflow to process
 * @err: Error code (0 if none)
 */
static void report_bootflow_err(struct bootflow *bflow, int err)
{
	if (!err)
		return;

	/* Indent out to 'Type' */
	printf("     ** ");

	switch (bflow->state) {
	case BOOTFLOWST_BASE:
		printf("No media/partition found");
		break;
	case BOOTFLOWST_MEDIA:
		printf("No partition found");
		break;
	case BOOTFLOWST_PART:
		printf("No filesystem found");
		break;
	case BOOTFLOWST_FS:
		printf("File not found");
		break;
	case BOOTFLOWST_FILE:
		printf("File cannot be loaded");
		break;
	case BOOTFLOWST_LOADED:
		printf("File loaded");
		break;
	case BOOTFLOWST_COUNT:
		break;
	}

	printf(", err=%d\n", err);
}

/**
 * show_bootflow() - Show the status of a bootflow
 *
 * @seq: Bootflow index
 * @bflow: Bootflow to show
 * @errors: True to show the error received, if any
 */
static void show_bootflow(int index, struct bootflow *bflow, bool errors)
{
	printf("%3x  %-11s  %-6s  %4x  %-14s  %s\n", index,
	       bootmethod_type_get_name(bflow->type),
	       bootmethod_state_get_name(bflow->state), bflow->part,
	       bflow->name, bflow->fname);
	if (errors)
		report_bootflow_err(bflow, bflow->err);
}

static void show_header(void)
{
	printf("Seq  Type         State   Part  Name            Filename\n");
	printf("---  -----------  ------  ----  --------------  ----------------\n");
}

static void show_footer(int count, int num_valid)
{
	printf("---  -----------  ------  ----  --------------  ----------------\n");
	printf("(%d bootflow%s, %d valid)\n", count, count != 1 ? "s" : "",
	       num_valid);
}

static int do_bootflow_list(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct bootflow_state *state;
	struct udevice *dev;
	struct bootflow *bflow;
	int num_valid = 0;
	bool errors = false;
	int ret, i;

	if (argc > 1 && *argv[1] == '-')
		errors = strchr(argv[1], 'e');

	ret = bootmethod_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
	dev = state->cur_bootmethod;
	if (dev) {
		printf("Showing bootflows for bootmethod '%s'\n", dev->name);
		show_header();
		for (ret = bootmethod_first_bootflow(dev, &bflow), i = 0;
		     !ret;
		     ret = bootmethod_next_bootflow(&bflow), i++) {
			num_valid += bflow->state == BOOTFLOWST_LOADED;
			show_bootflow(i, bflow, errors);
		}
	} else {
		printf("Showing all bootflows\n");
		show_header();
		for (ret = bootflow_first_glob(&bflow), i = 0;
		     !ret;
		     ret = bootflow_next_glob(&bflow), i++) {
			num_valid += bflow->state == BOOTFLOWST_LOADED;
			show_bootflow(i, bflow, errors);
		}
	}
	show_footer(i, num_valid);

	return 0;
}

static int do_bootflow_scan(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct bootflow_state *state;
	struct bootmethod_iter iter;
	struct udevice *dev;
	struct bootflow bflow;
	bool list = false, all = false, errors = false;
	int num_valid = 0;
	int ret, i;

	if (argc > 1 && *argv[1] == '-') {
		list = strchr(argv[1], 'l');
		all = strchr(argv[1], 'a');
		errors = strchr(argv[1], 'e');
	}

	ret = bootmethod_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
	dev = state->cur_bootmethod;
	if (dev) {
		if (list)
			printf("Scanning for bootflows in bootmethod '%s'\n",
			       dev->name);
		show_header();
		bootmethod_clear_bootflows(dev);
		for (i = 0, ret = 0; i < 100 && ret != -ESHUTDOWN; i++) {
			ret = bootmethod_get_bootflow(dev, i, &bflow);
			if ((ret && !all) || ret == -ESHUTDOWN) {
				/* TODO(sjg@chromium.org): free bflow fields */
				continue;
			}
			bflow.err = ret;
			ret = bootmethod_add_bootflow(&bflow);
			if (ret) {
				printf("Out of memory\n");
				return CMD_RET_FAILURE;
			}
			num_valid++;
			if (list)
				show_bootflow(i, &bflow, errors);
		}
	} else {
		int flags = 0;

		if (list)
			printf("Scanning for bootflows in all bootmethods\n");
		show_header();
		bootmethod_clear_glob();
		if (list)
			flags |= BOOTFLOWF_SHOW;
		if (all)
			flags |= BOOTFLOWF_ALL;
		for (i = 0,
		     ret = bootmethod_scan_first_bootflow(&iter, flags, &bflow);
		     i < 1000 && ret != -ENODEV;
	             i++, ret = bootmethod_scan_next_bootflow(&iter, &bflow)) {
			bflow.err = ret;
			if (!ret)
				num_valid++;
			ret = bootmethod_add_bootflow(&bflow);
			if (ret) {
				printf("Out of memory\n");
				return CMD_RET_FAILURE;
			}
			if (list)
				show_bootflow(i, &bflow, errors);
		}
	}
	if (list)
		show_footer(i, num_valid);

	return 0;
}

static int do_bootflow_select(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	struct bootflow_state *state;
	struct bootflow *bflow, *found;
	struct udevice *dev;
	const char *name;
	char *endp;
	int seq, i;
	int ret;

	ret = bootmethod_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
;
	if (argc < 2) {
		state->cur_bootflow = NULL;
		return 0;
	}
	dev = state->cur_bootmethod;

	name = argv[1];
	seq = simple_strtol(name, &endp, 16);
	found = NULL;

	if (dev) {
		for (ret = bootmethod_first_bootflow(dev, &bflow), i = 0;
		     !ret;
		     ret = bootmethod_next_bootflow(&bflow), i++) {
			if (*endp ? !strcmp(bflow->name, name) : (i == seq)) {
				found = bflow;
				break;
			}
		}
	} else {
		for (ret = bootflow_first_glob(&bflow), i = 0;
		     !ret;
		     ret = bootflow_next_glob(&bflow), i++) {
			if (*endp ? !strcmp(bflow->name, name) : (i == seq)) {
				found = bflow;
				break;
			}
		}
	}

	if (!found) {
		printf("Cannot find bootflow '%s' ", name);
		if (dev)
			printf("in bootmethod '%s' ", dev->name);
		printf("(err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	state->cur_bootflow = found;

	return 0;
}

static int do_bootflow_info(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct bootflow_state *state;
	struct bootflow *bflow;
	bool dump = false;
	int ret;

	if (argc > 1 && *argv[1] == '-')
		dump = strchr(argv[1], 'd');

	ret = bootmethod_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;

	if (!state->cur_bootflow) {
		printf("No bootflow selected\n");
		return CMD_RET_FAILURE;
	}
	bflow = state->cur_bootflow;

	printf("Name:      %s\n", bflow->name);
	printf("Device:    %s\n", bflow->dev->name);
	printf("Block dev: %s\n", bflow->blk ? bflow->blk->name : "(none)");
	printf("Sequence:  %d\n", bflow->seq);
	printf("Type:      %s\n", bootmethod_type_get_name(bflow->type));
	printf("State:     %s\n", bootmethod_state_get_name(bflow->state));
	printf("Partition: %d\n", bflow->part);
	printf("Filename:  %s\n", bflow->fname);
	printf("Buffer:    %lx\n", (ulong)map_to_sysmem(bflow->buf));
	printf("Size:      %x (%d bytes)\n", bflow->size, bflow->size);
	printf("Error:     %d\n", bflow->err);
	printf("Contents:\n\n");
	if (dump && bflow->buf) {
		/* Set some sort of maximum on the size */
		int size = min(bflow->size, 10 << 10);
		int i;

		for (i = 0; i < size; i++) {
			putc(bflow->buf[i]);
			if (!(i % 128) && ctrlc()) {
				printf("...interrupted\n");
				break;
			}

		}
	}

	return 0;
}

static int do_bootflow_boot(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct bootflow_state *state;
	struct bootflow *bflow;
	int ret;

	ret = bootmethod_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;

	if (!state->cur_bootflow) {
		printf("No bootflow selected\n");
		return CMD_RET_FAILURE;
	}
	bflow = state->cur_bootflow;

	ret = bootflow_boot(bflow);
	switch (ret) {
	case -EPROTO:
		printf("Bootflow not loaded (state '%s')\n",
		       bootmethod_state_get_name(bflow->state));
		break;
	case -ENOSYS:
		printf("Boot type '%s' not supported\n",
		       bootmethod_type_get_name(bflow->type));
		break;
	default:
		printf("Boot failed (err=%d)\n", ret);
		break;
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootflow_help_text[] =
	"scan [-lae]  - scan for valid bootflows (-l list, -a all, -e errors))\n"
	"list [-e]    - list scanned bootflows (-e errors)\n"
	"select       - select a bootflow\n"
	"info [-d]    - show info on current bootflow (-d dump bootflow)\n"
	"boot         - boot current bootflow";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootflow, "Bootflows", bootflow_help_text,
	U_BOOT_SUBCMD_MKENT(scan, 2, 1, do_bootflow_scan),
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootflow_list),
	U_BOOT_SUBCMD_MKENT(select, 2, 1, do_bootflow_select),
	U_BOOT_SUBCMD_MKENT(info, 2, 1, do_bootflow_info),
	U_BOOT_SUBCMD_MKENT(boot, 1, 1, do_bootflow_boot));
