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
#include <malloc.h>
#include <linux/libfdt.h>

struct build_info {
	const char **str_for_id;
	int str_count;
};

/**
 * lookup_str - Look up a string in the table
 *
 * @ldtb: Lookup DTB
 * @find_name: Name to look for (e.g. "title")
 * @strp: Returns a pointer to the string value
 * Return: ID on success, or -ve on error
 */
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

/**
 * add_expo_str - Look up a string in the table and add to expo
 *
 * @ldtb: Lookup DTB
 * @exp: Expo to add to
 * @find_name: Name to look for (e.g. "title")
 * Return: Id of added string, or -ve on error
 */
int add_expo_str(void *ldtb, struct expo *exp, const char *find_name)
{
	const char *text;
	uint str_id;
	int ret;

	ret = lookup_str(ldtb, find_name, &text);
	if (ret < 0)
		return log_msg_ret("lu", ret);
	str_id = ret;

	ret = expo_str(exp, find_name, str_id, text);
	if (ret < 0)
		return log_msg_ret("add", ret);

	return ret;
}

/**
 * add_txt_str - Add a string or lookup its ID, then add to expo
 *
 * @ldtb: Lookup DTB
 * @scn: Scene to add to
 * @find_name: Name to look for (e.g. "title"). This will find a property called
 * "title" if it exists, else will look up the string for "title-id"
 * Return: Id of added string, or -ve on error
 */
int add_txt_str(void *ldtb, struct scene *scn, const char *find_name, uint obj_id)
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

/*
 * build_element() - Handle creating a text object from a label
 *
 * Look up a property called @label or @label-id and create a string for it
 */
int build_element(void *ldtb, int node, const char *label)
{
	return 0;
}

static int read_strings(struct build_info *info, const void *ldtb)
{
	int strings;
	int node;

	strings = fdt_subnode_offset(ldtb, 0, "strings");
	if (strings < 0)
		return log_msg_ret("str", -EINVAL);

	fdt_for_each_subnode(node, ldtb, strings) {
		const char *val;
		int id;

		id = fdtdec_get_int(ldtb, node, "id", 0);
		if (!id)
			return log_msg_ret("id", -EINVAL);
		val = fdt_getprop(ldtb, node, "value", NULL);
		if (!val)
			return log_msg_ret("val", -EINVAL);

		if (id >= info->str_count) {
			int new_count = info->str_count + 20;
			void *new_arr;

			new_arr = realloc(info->str_for_id,
					  new_count * sizeof(char *));
			if (!new_arr)
				return log_msg_ret("id", -ENOMEM);
			memset(new_arr + info->str_count, '\0',
			       (new_count - info->str_count) * sizeof(char *));
			info->str_for_id = new_arr;
			info->str_count = new_count;
		}

		info->str_for_id[id] = val;
	}

	return 0;
}

int expo_build(void *ldtb, struct expo **expp)
{
	struct build_info info;
	struct expo *exp;
	int scenes, node;
	int ret;

	memset(&info, '\0', sizeof(info));
	ret = read_strings(&info, ldtb);
	if (ret)
		return log_msg_ret("str", ret);

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
		id = fdtdec_get_int(ldtb, node, "id", 0);
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
