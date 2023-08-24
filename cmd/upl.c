// SPDX-License-Identifier: GPL-2.0+
/*
 * Commands for UPL handoff generation
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY UCLASS_BOOTSTD

#include <command.h>
#include <abuf.h>
#include <display_options.h>
#include <mapmem.h>
#include <string.h>
#include <upl.h>
#include <dm/ofnode.h>

static int do_upl_write(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct upl s_upl, *upl = &s_upl;
	struct abuf buf;
	oftree tree;
	ulong addr;
	int ret;

	printf("upl size %lx\n", sizeof(struct upl));
	upl_get_test_data(upl);

	log_debug("Writing UPL\n");
	ret = upl_create_handoff_tree(upl, &tree);
	if (ret) {
		log_err("Failed to write (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	log_debug("Flattening\n");
	ret = oftree_to_fdt(tree, &buf);
	if (ret) {
		log_err("Failed to write (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}
	addr = map_to_sysmem(abuf_data(&buf));
	printf("UPL handoff written to %lx size %lx\n", addr, abuf_size(&buf));
	if (env_set_hex("upladdr", addr) ||
	    env_set_hex("uplsize", abuf_size(&buf))) {
		printf("Cannot set env var\n");
		return CMD_RET_FAILURE;
	}

	log_debug("done\n");

	return 0;
}

static int do_upl_read(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct upl s_upl, *upl = &s_upl;
	oftree tree;
	ulong addr;
	int ret;

	if (argc < 1)
		return CMD_RET_USAGE;
	addr = hextoul(argv[1], NULL);

	printf("Reading UPL at %lx\n", addr);
	tree = oftree_from_fdt(map_sysmem(addr, 0));
	ret = upl_read_handoff(upl, tree);
	if (ret) {
		log_err("Failed to read (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char upl_help_text[] =
	"read <addr>  - Read handoff information\n"
	"write        - Write handoff information";
#endif

U_BOOT_CMD_WITH_SUBCMDS(upl, "Universal Payload support", upl_help_text,
	U_BOOT_SUBCMD_MKENT(read, 2, 1, do_upl_read),
	U_BOOT_SUBCMD_MKENT(write, 1, 1, do_upl_write));
