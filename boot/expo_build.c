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

int lookup_str(void *ldtb, const char *find_name, const char **strp)
{
	int strings;
	int node;

	strings = fdt_subnode_offset(ldtb, 0, "strings");
	if (strings < 0)
		return log_msg_ret("str", -EINVAL);

	fdt_for_each_subnode(node, ldtb, strings) {
		const char *val, *name;
		int id;

		name = fdt_get_name(ldtb, node, NULL);
		if (strcmp(name, find_name))
			continue;

		id = fdtdec_get_int(ldtb, node, "id", 0);
		if (!id)
			return log_msg_ret("id", -EINVAL);
		val = fdt_getprop(ldtb, node, "value", NULL);
		if (!val)
			return log_msg_ret("val", -EINVAL);
		*strp = val;

		return id;
	}

	printf("string '%s' not found\n", find_name);
	return log_msg_ret("lo", -ENOENT);
}

int add_expo_str(void *ldtb, struct expo *exp, const char *name)
{
	const char *text;
	uint str_id;
	int ret;

	ret = lookup_str(ldtb, name, &text);
	if (ret < 0)
		return log_msg_ret("lu", ret);
	str_id = ret;

	ret = expo_str(exp, name, str_id, text);
	if (ret < 0)
		return log_msg_ret("add", ret);

	return ret;
}

int add_txt_str(void *ldtb, struct scene *scn, const char *name, uint obj_id)
{
	const char *text;
	uint str_id;
	int ret;

	ret = lookup_str(ldtb, name, &text);
	if (ret < 0)
		return log_msg_ret("lu", ret);
	str_id = ret;

	ret = scene_txt_str(scn, name, obj_id, str_id, text, NULL);
	if (ret < 0)
		return log_msg_ret("add", ret);

	return ret;
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
		const char *name;
		struct scene *scn;
		uint id, title_id;

		name = fdt_get_name(ldtb, node, NULL);
		id = fdtdec_get_int(ldtb, node, "scene-id", 0);
		if (!id)
			return log_msg_ret("id", -EINVAL);

		ret = scene_new(exp, name, id, &scn);
		if (ret < 0)
			return log_msg_ret("scn", ret);

		ret = add_txt_str(ldtb, scn, "title", 0);
		if (ret < 0)
			return log_msg_ret("tit", -EINVAL);
		title_id = ret;
		scene_title_set(scn, title_id);

		ret = add_txt_str(ldtb, scn, "prompt", 0);
		if (ret < 0)
			return log_msg_ret("pr", ret);
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
