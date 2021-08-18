// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootmeth' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <command.h>
#include <dm.h>
#include <malloc.h>
#include <dm/uclass-internal.h>

static int do_bootmeth_list(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct bootstd_priv *std;
	struct udevice *dev;
	bool use_order;
	bool all = false;
	int ret;
	int i;

	if (argc > 1 && *argv[1] == '-') {
		all = strchr(argv[1], 'a');
		argc--;
		argv++;
	}

	ret = bootstd_get_priv(&std);
	if (ret) {
		printf("Cannot get bootstd (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Order  Seq  Name                Description\n");
	printf("-----  ---  ------------------  ------------------\n");

	/*
	 * Use the ordering if we have one, so long as we are not trying to list
	 * all bootmethds
	 */
	use_order = std->bootmeth_count && !all;
	if (use_order)
		dev = std->bootmeth_order[0];
	else
		ret = uclass_find_first_device(UCLASS_BOOTMETH, &dev);

	for (i = 0; dev;) {
		struct bootmeth_uc_plat *ucp = dev_get_uclass_plat(dev);
		int order = i;

		/*
		 * With the -a flag we may list bootdevs that are not in the
		 * ordering. Find their place in the order
		 */
		if (all && std->bootmeth_count) {
			int j;

			/* Find the position of this bootmeth in the order */
			order = -1;
			for (j = 0; j < std->bootmeth_count; j++) {
				if (std->bootmeth_order[j] == dev)
					order = j;
			}
		}

		if (order == -1)
			printf("%5s", "-");
		else
			printf("%5x", order);
		printf("  %3x  %-19.19s %s\n", dev_seq(dev), dev->name,
		       ucp->desc);
		i++;
		if (use_order)
			dev = std->bootmeth_order[i];
		else
			uclass_find_next_device(&dev);
	}
	printf("-----  ---  ------------------  ------------------\n");
	printf("(%d bootmeth%s)\n", i, i != 1 ? "s" : "");

	return 0;
}

static int bootmeth_order(int argc, char *const argv[])
{
	struct bootstd_priv *std;
	struct udevice **order;
	int count, ret, i;

	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;

	if (!argc) {
		free(std->bootmeth_order);
		std->bootmeth_order = NULL;
		std->bootmeth_count = 0;
		return 0;
	}

	/* Create an array large enough */
	count = uclass_id_count(UCLASS_BOOTMETH);
	if (!count)
		return log_msg_ret("count", -ENOENT);

	order = calloc(max(argc, count) + 1, sizeof(struct udevice *));
	if (!order)
		return log_msg_ret("order", -ENOMEM);

	for (i = 0; i < argc; i++) {
		struct udevice *dev;

		ret = uclass_find_device_by_name(UCLASS_BOOTMETH, argv[i],
						 &dev);
		if (ret) {
			printf("Unknown bootmeth '%s'\n", argv[i]);
			free(order);
			return ret;
		}
		order[i] = dev;
	}
	order[i] = NULL;
	free(std->bootmeth_order);
	std->bootmeth_order = order;
	std->bootmeth_count = i;

	return 0;
}

static int do_bootmeth_order(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	int ret;

	ret = bootmeth_order(argc - 1, argv + 1);
	if (ret) {
		printf("Failed (err=%d)\n", ret);
		return CMD_RET_FAILURE;
        }

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootmeth_help_text[] =
	"list [-a]     - list available bootmeths (-a all)\n"
	"bootmeth order [<bd> ...]  - select bootmeth order / subset to use";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootmeth, "Boot methods", bootmeth_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootmeth_list),
	U_BOOT_SUBCMD_MKENT(order, CONFIG_SYS_MAXARGS, 1, do_bootmeth_order));
