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
#include <dm/ofnode.h>
#include <linux/libfdt.h>

/**
 * struct build_info - Information to use when building
 *
 * @str_for_id: String for each ID in use, NULL if empty. The string is NULL
 *	if there is nothing for this ID. Since ID 0 is never used, the first
 *	element of this array is always NULL
 * @str_count: Number of entries in @str_for_id
 */
struct build_info {
	const char **str_for_id;
	int str_count;
};

#if 0
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
#endif

/**
 * add_txt_str - Add a string or lookup its ID, then add to expo
 *
 * @info: Build information
 * @node: Node describing scene
 * @scn: Scene to add to
 * @find_name: Name to look for (e.g. "title"). This will find a property called
 * "title" if it exists, else will look up the string for "title-id"
 * Return: Id of added string, or -ve on error
 */
int add_txt_str(struct build_info *info, ofnode node, struct scene *scn,
		const char *find_name, uint obj_id)
{
	const char *text;
	uint str_id;
	int ret;

	text = ofnode_read_string(node, find_name);
	if (!text) {
		char name[40];
		u32 id;

		snprintf(name, sizeof(name), "%s-id", find_name);
		ret = ofnode_read_u32(node, name, &id);
		if (ret)
			return log_msg_ret("id", -EINVAL);

		if (id >= info->str_count)
			return log_msg_ret("id", -E2BIG);
		text = info->str_for_id[id];
		if (!text)
			return log_msg_ret("id", -EINVAL);
	}

	ret = expo_str(scn->expo, find_name, 0, text);
	if (ret < 0)
		return log_msg_ret("add", ret);
	str_id = ret;

	ret = scene_txt_str(scn, find_name, obj_id, str_id, text, NULL);
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

static int read_strings(struct build_info *info, oftree tree)
{
	ofnode strings, node;

	strings = oftree_path(tree, "/strings");
	if (!ofnode_valid(strings))
		return log_msg_ret("str", -EINVAL);

	ofnode_for_each_subnode(node, strings) {
		const char *val;
		int ret;
		u32 id;

		ret = ofnode_read_u32(node, "id", &id);
		if (ret)
			return log_msg_ret("id", -EINVAL);
		val = ofnode_read_string(node, "value");
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

static void list_strings(struct build_info *info)
{
	int i;

	for (i = 0; i < info->str_count; i++) {
		if (info->str_for_id[i])
			printf("%3d %s\n", i, info->str_for_id[i]);
	}
}

static int menu_build(struct build_info *info, ofnode node, struct scene *scn)
{
	struct scene_obj_menu *menu;
	const char *name;
	uint title_id, menu_id;
	u32 id;
	int ret;

	name = ofnode_get_name(node);
	ret = ofnode_read_u32(node, "id", &id);
	if (ret)
		return log_msg_ret("id", -EINVAL);

	ret = scene_menu(scn, name, id, &menu);
	if (ret < 0)
		return log_msg_ret("men", ret);
	menu_id = ret;

	/* Set the title */
	ret = add_txt_str(info, node, scn, "title", 0);
	if (ret < 0)
		return log_msg_ret("tit", ret);
	title_id = ret;
	ret = scene_menu_set_title(scn, menu_id, title_id);



// 	id = scene_menuitem(scn, menu_id, "linux", ITEM1, ITEM1_KEY,
// 			    ITEM1_LABEL, ITEM1_DESC, ITEM1_PREVIEW, 0, &item);

	return 0;
}

static int item_build(struct build_info *info, ofnode node, struct scene *scn)
{
	const char *type;
	u32 id;
	int ret;

	ret = ofnode_read_u32(node, "id", &id);
	if (ret)
		return log_msg_ret("id", -EINVAL);

	type = ofnode_read_string(node, "type");
	if (!type)
		return log_msg_ret("typ", -ENOENT);

	if (!strcmp("menu", type)) {
		ret = menu_build(info, node, scn);
	} else {
		ret = -EINVAL;
	}
	if (ret)
		return log_msg_ret("typ", ret);

	return 0;
}

static int scene_build(struct build_info *info, ofnode scn_node,
		       struct expo *exp)
{
	const char *name;
	struct scene *scn;
	uint id, title_id;
	ofnode node;
	int ret;

	name = ofnode_get_name(scn_node);
	ret = ofnode_read_u32(scn_node, "id", &id);
	if (ret)
		return log_msg_ret("id", -EINVAL);

	ret = scene_new(exp, name, id, &scn);
	if (ret < 0)
		return log_msg_ret("scn", ret);

	ret = add_txt_str(info, scn_node, scn, "title", 0);
	if (ret < 0)
		return log_msg_ret("tit", ret);
	title_id = ret;
	scene_title_set(scn, title_id);

	ret = add_txt_str(info, scn_node, scn, "prompt", 0);
	if (ret < 0)
		return log_msg_ret("pr", ret);

	ofnode_for_each_subnode(node, scn_node) {
		ret = item_build(info, node, scn);
		if (ret < 0)
			return log_msg_ret("itm", ret);
	}

	return 0;
}

int expo_build(oftree tree, struct expo **expp)
{
	struct build_info info;
	struct expo *exp;
	ofnode scenes, node;
	int ret;

	memset(&info, '\0', sizeof(info));
	ret = read_strings(&info, tree);
	if (ret)
		return log_msg_ret("str", ret);
	list_strings(&info);

	ret = expo_new("name", NULL, &exp);
	if (ret)
		return log_msg_ret("exp", ret);

	scenes = oftree_path(tree, "/scenes");
	if (!ofnode_valid(scenes))
		return log_msg_ret("sno", -EINVAL);

	ofnode_for_each_subnode(node, scenes) {
		ret = scene_build(&info, node, exp);
		if (ret < 0)
			return log_msg_ret("scn", ret);
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
