// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootmeth' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmeth.h>
#include <command.h>
#include <dm.h>
#include <dm/uclass-internal.h>

static int do_bootmeth_list(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct udevice *dev;
	int ret;
	int i;

	printf("Seq  Name                Description\n");
	printf("---  ------------------  ------------------\n");
	ret = uclass_find_first_device(UCLASS_BOOTMETH, &dev);
	for (i = 0; dev; i++) {
		struct bootmeth_uc_plat *ucp = dev_get_uclass_plat(dev);

		printf("%3x  %-19.19s %s\n", dev_seq(dev), dev->name,
		       ucp->desc);
		ret = uclass_find_next_device(&dev);
	}
	printf("---  ------------------  ------------------\n");
	printf("(%d bootmeth%s)\n", i, i != 1 ? "s" : "");

	return 0;
}

static int do_bootmeth_allow(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootmeth_help_text[] =
	"list [-a]     - list available bootmeths (-a all)\n"
	"bootmeth allow [<bd>]  - select bootmeths to be used for booting";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootmeth, "Boot methods", bootmeth_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootmeth_list),
	U_BOOT_SUBCMD_MKENT(allow, 2, 1, do_bootmeth_allow));
