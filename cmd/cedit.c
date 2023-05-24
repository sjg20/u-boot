// SPDX-License-Identifier: GPL-2.0+
/*
 * 'cedit' command
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <cli.h>
#include <command.h>
#include <dm.h>
#include <expo.h>
#include <fs.h>
#include <menu.h>
#include <dm/ofnode.h>
#include <linux/delay.h>
#include <linux/sizes.h>

struct expo *cur_exp;

static int do_cedit_load(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	const char *fname;
	struct expo *exp;
	oftree tree;
	ulong size;
	void *buf;
	int ret;

	if (argc < 4)
		return CMD_RET_USAGE;
	fname = argv[3];

	ret = fs_load_alloc(argv[1], argv[2], argv[3], SZ_1M, 0, &buf, &size);
	if (ret) {
		printf("File not found\n");
		return CMD_RET_FAILURE;
	}

	tree = oftree_from_fdt(buf);
	if (!oftree_valid(tree)) {
		printf("Cannot create oftree\n");
		return CMD_RET_FAILURE;
	}

	ret = expo_build(tree, &exp);
	oftree_dispose(tree);
	if (ret) {
		printf("Failed to build expo: %dE\n", ret);
		return CMD_RET_FAILURE;
	}

	cur_exp = exp;

	return 0;
}


static int cedit_run(struct expo *exp)
{
	struct cli_ch_state s_cch, *cch = &s_cch;
	struct udevice *dev;
	uint sel_id;
	bool done;
	int ret;

	cli_ch_init(cch);
/*
	if (ofnode_valid(std->theme)) {
		ret = bootflow_menu_apply_theme(exp, std->theme);
		if (ret)
			return log_msg_ret("thm", ret);
	}
*/
	/* For now we only support a video console */
	ret = uclass_first_device_err(UCLASS_VIDEO, &dev);
	if (ret)
		return log_msg_ret("vid", ret);
	ret = expo_set_display(exp, dev);
	if (ret)
		return log_msg_ret("dis", ret);

	ret = expo_first_scene_id(exp);
	if (ret < 0)
		return log_msg_ret("scn", ret);

	ret = expo_set_scene_id(exp, ret);
	if (ret)
		return log_msg_ret("scn", ret);

// 	if (text_mode)
// 		exp_set_text_mode(exp, text_mode);

	done = false;
	do {
		struct expo_action act;
		int ichar, key;

		ret = expo_render(exp);
		if (ret)
			break;

		ichar = cli_ch_process(cch, 0);
		if (!ichar) {
			while (!ichar && !tstc()) {
				schedule();
				mdelay(2);
				ichar = cli_ch_process(cch, -ETIMEDOUT);
			}
			if (!ichar) {
				ichar = getchar();
				ichar = cli_ch_process(cch, ichar);
			}
		}

		key = 0;
		if (ichar) {
			key = bootmenu_conv_key(ichar);
			if (key == BKEY_NONE)
				key = ichar;
		}
		if (!key)
			continue;

		ret = expo_send_key(exp, key);
		if (ret)
			break;

		ret = expo_action_get(exp, &act);
		if (!ret) {
			switch (act.type) {
			case EXPOACT_SELECT:
				sel_id = act.select.id;
				done = true;
				break;
			case EXPOACT_QUIT:
				done = true;
				break;
			default:
				break;
			}
		}
	} while (!done);

	if (ret)
		return log_msg_ret("end", ret);

	return 0;
}

static int do_cedit_run(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	ofnode node;
	int ret;

	if (!cur_exp) {
		printf("No expo loaded\n");
		return CMD_RET_FAILURE;
	}

	node = ofnode_path("/cedit-theme");
	if (ofnode_valid(node)) {
		ret = expo_apply_theme(cur_exp, node);
		if (ret)
			return CMD_RET_FAILURE;
	} else {
		log_warning("No theme found\n");
	}
	ret = cedit_run(cur_exp);
	if (ret)
		return CMD_RET_FAILURE;

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char cedit_help_text[] =
	"load <interface> <dev[:part]> <filename>   - load config editor\n"
	"cedit run                                        - run config editor";
#endif /* CONFIG_SYS_LONGHELP */

U_BOOT_CMD_WITH_SUBCMDS(cedit, "Configuration editor", cedit_help_text,
	U_BOOT_SUBCMD_MKENT(load, 5, 1, do_cedit_load),
	U_BOOT_SUBCMD_MKENT(run, 1, 1, do_cedit_run),
);
