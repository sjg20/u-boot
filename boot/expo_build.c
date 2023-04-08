// SPDX-License-Identifier: GPL-2.0+
/*
 * Building an expo from an FDT description
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <expo.h>
#include <fdtdec.h>
#include <linux/libfdt.h>

const char *lookup_str(void *ldtb, const char *find_name)
{
	int strings;
	int node;

	strings = fdt_subnode_offset(ldtb, 0, "strings");
	if (strings < 0) {
		log_msg_ret("str", -EINVAL);
		return NULL;
	}

	fdt_for_each_subnode(node, ldtb, strings) {
		const char *val, *name;
		int id;

		name = fdt_get_name(ldtb, node, NULL);
		if (strcmp(name, find_name))
			continue;

		id = fdtdec_get_int(ldtb, node, "id", 0);
		if (!id) {
			log_msg_ret("id", -EINVAL);
			return NULL;
		}
		val = fdt_getprop(ldtb, node, "value", NULL);
		if (!val) {
			log_msg_ret("val", -EINVAL);
			return NULL;
		}

		return val;
	}

	printf("string '%s' not found\n", find_name);
	log_msg_ret("lo", -ENOENT);

	return NULL;
}

int expo_build(void *ldtb, struct expo **expp)
{
	struct expo *exp;
	int scenes, node;
	int ret;

	ret = expo_new("name", NULL, &exp);
	if (ret)
		return log_msg_ret("exp", ret);

	scenes = fdt_subnode_offset(ldtb, 0, "scenes");
	if (scenes < 0)
		return log_msg_ret("scn", -EINVAL);

	fdt_for_each_subnode(node, ldtb, scenes) {
		const char *title_name, *name;
		struct scene *scn;
		int id;

		name = fdt_get_name(ldtb, node, NULL);
		id = fdtdec_get_int(ldtb, node, "scene-id", 0);
		if (!id)
			return log_msg_ret("id", -EINVAL);
		title_name = fdt_getprop(ldtb, node, "title", NULL);
		if (!title_name)
			return log_msg_ret("tit", -EINVAL);

		ret = scene_new(exp, lookup_str(ldtb, title_name), id, &scn);
		if (ret < 0)
			return log_msg_ret("scn", ret);

		ret = scene_title_set(scn, title_name);
		if (ret < 0)
			return log_msg_ret("sc", ret);

// 		ret |= scene_txt_str(scn, "prompt", OBJ_PROMPT, STR_PROMPT,
// 				"UP and DOWN to choose, ENTER to select", NULL);
	}
#if 0
	ret = scene_menu(scn, "main", OBJ_MENU, &menu);
	ret |= scene_obj_set_pos(scn, OBJ_MENU, MARGIN_LEFT, 100);
	ret |= scene_txt_str(scn, "title", OBJ_MENU_TITLE, STR_MENU_TITLE,
			     "U-Boot - Boot Menu", NULL);
	ret |= scene_menu_set_title(scn, OBJ_MENU, OBJ_PROMPT);

	logo = video_get_u_boot_logo();
	if (logo) {
		ret |= scene_img(scn, "ulogo", OBJ_U_BOOT_LOGO, logo, NULL);
		ret |= scene_obj_set_pos(scn, OBJ_U_BOOT_LOGO, -4, 4);
	}

	ret |= scene_txt_str(scn, "cur_item", OBJ_POINTER, STR_POINTER, ">",
			     NULL);
	ret |= scene_menu_set_pointer(scn, OBJ_MENU, OBJ_POINTER);
	if (ret < 0)
		return log_msg_ret("new", -EINVAL);
#endif

	return 0;
}
