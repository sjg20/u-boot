// SPDX-License-Identifier: GPL-2.0+
/*
 * Provide a menu of available bootflows and related options
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <common.h>
#include <bootflow.h>
#include <bootstd.h>
#include <cli.h>
#include <dm.h>
#include <expo.h>
#include <menu.h>
#include <watchdog.h>
#include <linux/delay.h>

enum {
	START,
	MAIN,

	OBJ_MENU,
	OBJ_MENU_TITLE,
	CUR_ITEM_TEXT,

	ITEM = 100,
	ITEM_TEXT = 200,
	ITEM_KEY = 300,
};

int bootflow_menu_new(struct expo **expp)
{
	struct scene_obj_menu *menu;
	struct bootflow *bflow;
	struct scene *scn;
	struct expo *exp;
	int ret, i;

	ret = expo_new("bootflows", &exp);
	if (ret)
		return log_msg_ret("exp", ret);
	ret = scene_new(exp, "main", MAIN, &scn);
	if (ret < 0)
		return log_msg_ret("scn", ret);
	ret = scene_menu_add(scn, "main", OBJ_MENU, &menu);
	ret |= scene_txt_add(scn, "title", OBJ_MENU_TITLE, "Main Menu", NULL);
	ret |= scene_menu_set_title(scn, OBJ_MENU, OBJ_MENU_TITLE);
	ret |= scene_txt_add(scn, "cur_item", CUR_ITEM_TEXT, ">", NULL);
	ret |= scene_menu_set_pointer(scn, OBJ_MENU, CUR_ITEM_TEXT);
	if (ret < 0)
		return log_msg_ret("new", -EINVAL);

	for (ret = bootflow_first_glob(&bflow), i = 0; !ret && i < 36;
	     ret = bootflow_next_glob(&bflow), i++) {
		char str[2], *key;

		if (bflow->state != BOOTFLOWST_READY)
			continue;

		*str = i < 10 ? '0' + i : 'A' + i - 10;
		str[1] = '\0';
		key = strdup(str);
		if (!key)
			return log_msg_ret("key", -ENOMEM);

		ret = scene_txt_add(scn, "txt", ITEM_TEXT + i, bflow->name,
				    NULL);
		ret |= scene_txt_add(scn, "key", ITEM_KEY + i, str, NULL);
		ret |= scene_menuitem_add(scn, OBJ_MENU, "item", ITEM + i,
					  ITEM_KEY + i, ITEM_TEXT + i, 0, NULL);
		if (ret < 0)
			return log_msg_ret("itm", -EINVAL);
		ret = 0;
	}

	*expp = exp;

	return 0;
}

int bootflow_menu_run(struct bootstd_priv *std, struct bootflow **bflowp)
{
	struct cli_ch_state s_cch, *cch = &s_cch;
	struct bootflow *sel_bflow;
	struct udevice *dev;
	struct expo *exp;
	uint sel_id;
	bool done;
	int ret;

	cli_ch_init(cch);

	sel_bflow = NULL;
	*bflowp = NULL;

	ret = bootflow_menu_new(&exp);
	if (ret)
		return log_msg_ret("exp", ret);

	/* For now we only support a video console */
	ret = uclass_first_device_err(UCLASS_VIDEO, &dev);
	if (ret)
		return log_msg_ret("vid", ret);
	ret = expo_set_display(exp, dev);
	if (ret)
		return log_msg_ret("dis", ret);

	ret = expo_set_scene_id(exp, MAIN);
	if (ret)
		return log_msg_ret("scn", ret);

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
				WATCHDOG_RESET();
				mdelay(10);
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

		printf("%d\n", key);

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
			default:
				break;
			}
		}
	} while (!done);

	if (ret)
		return log_msg_ret("end", ret);

	if (sel_id) {
		struct bootflow *bflow;
		int i;

		for (ret = bootflow_first_glob(&bflow), i = 0; !ret && i < 36;
		     ret = bootflow_next_glob(&bflow), i++) {
			if (i == sel_id - ITEM) {
				sel_bflow = bflow;
				break;
			}
		}
	}

	expo_destroy(exp);

	if (!sel_bflow)
		return -EAGAIN;
	*bflowp = sel_bflow;

	return 0;
}
