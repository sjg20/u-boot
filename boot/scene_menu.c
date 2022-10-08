// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of a menu in a scene
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY	LOGC_BOOT

#include <common.h>
#include <dm.h>
#include <expo.h>
#include <malloc.h>
#include <mapmem.h>
#include <menu.h>
#include <video.h>
#include <video_console.h>
#include <linux/input.h>
#include "scene_internal.h"

static void scene_menuitem_destroy(struct scene_menuitem *item)
{
	free(item->name);
	free(item);
}

void scene_menu_destroy(struct scene_obj_menu *menu)
{
	struct scene_menuitem *item, *next;

	list_for_each_entry_safe(item, next, &menu->item_head, sibling)
		scene_menuitem_destroy(item);
}

/**
 * menu_point_to_item() - Point to a particular menu item
 *
 * Sets the currently pointed-to / highlighted menu item
 */
static void menu_point_to_item(struct scene_obj_menu *menu, uint item_id)
{
	menu->cur_item_id = item_id;
}

int scene_menu_arrange(struct scene *scn, struct scene_obj_menu *menu)
{
	struct scene_menuitem *item;
	int y, cur_y, first_y;
	int ret;

	y = menu->obj.y;
	if (menu->title_id) {
		ret = scene_obj_set_pos(scn, menu->title_id, menu->obj.x, y);
		if (ret < 0)
			return log_msg_ret("tit", ret);

		ret = scene_obj_get_hw(scn, menu->title_id, NULL);
		if (ret < 0)
			return log_msg_ret("hei", ret);

		y += ret;
	}

	cur_y = -1;
	first_y = -1;
	list_for_each_entry(item, &menu->item_head, sibling) {
		int height;

		/* select an item if not done already */
		if (!menu->cur_item_id)
			menu_point_to_item(menu, item->id);
		ret = scene_obj_set_pos(scn, item->text_id, menu->obj.x + 100,
					y);
		if (ret < 0)
			return log_msg_ret("txt", ret);

		ret = scene_obj_set_pos(scn, item->key_id, menu->obj.x, y);
		if (ret < 0)
			return log_msg_ret("key", ret);

		ret = scene_obj_get_hw(scn, item->text_id, NULL);
		if (ret < 0)
			return log_msg_ret("get", ret);
		height =ret;

		if (menu->cur_item_id == item->id)
			cur_y = y;
		if (first_y == -1)
			first_y = y;

		if (item->preview_id) {
			bool hide;

			/*
			 * put all previews on top of each other, to the right
			 * of the items and level with the first one
			 */
			ret = scene_obj_set_pos(scn, item->preview_id,
						menu->obj.x + 400, first_y);
			if (ret < 0)
				return log_msg_ret("prev", ret);

			hide = menu->cur_item_id != item->id;
			ret = scene_obj_set_hide(scn, item->preview_id, hide);
			if (ret < 0)
				return log_msg_ret("hid", ret);
		}

		y += height;
	}

	if (menu->pointer_id && cur_y != -1) {
		/*
		 * put the pointer to the right of and level with the item it
		 * points to
		 */
		ret = scene_obj_set_pos(scn, menu->pointer_id,
					menu->obj.x + 50, cur_y);
		if (ret < 0)
			return log_msg_ret("ptr", ret);
	}

	return 0;
}

int scene_menu_add(struct scene *scn, const char *name, uint id,
		   struct scene_obj_menu **menup)
{
	struct scene_obj_menu *menu;
	int ret;

	ret = scene_obj_add(scn, name, id, SCENEOBJT_MENU,
			    sizeof(struct scene_obj_menu),
			    (struct scene_obj **)&menu);
	if (ret < 0)
		return log_msg_ret("obj", -ENOMEM);

	if (menup)
		*menup = menu;
	INIT_LIST_HEAD(&menu->item_head);

	ret = scene_menu_arrange(scn, menu);
	if (ret)
		return log_msg_ret("pos", ret);

	return menu->obj.id;
}

int scene_menu_send_key(struct scene_obj_menu *menu, int key,
			struct expo_action *event)
{
	struct scene_menuitem *item, *cur;

	cur = NULL;

	if (!list_empty(&menu->item_head)) {
		list_for_each_entry(item, &menu->item_head, sibling) {
			/* select an item if not done already */
			if (menu->cur_item_id == item->id) {
				cur = item;
				break;
			}
		}
	}

	if (!cur)
		return -ENOTTY;

	switch (key) {
	case BKEY_UP:
		if (item != list_first_entry(&menu->item_head,
					     struct scene_menuitem, sibling)) {
			item = list_entry(item->sibling.prev,
					  struct scene_menuitem, sibling);
			event->type = EXPOACT_POINT;
			event->select.id = item->id;
			log_debug("up to item %d\n", event->select.id);
		}
		break;
	case BKEY_DOWN:
		if (!list_is_last(&item->sibling, &menu->item_head)) {
			item = list_entry(item->sibling.next,
					  struct scene_menuitem, sibling);
			event->type = EXPOACT_POINT;
			event->select.id = item->id;
			log_debug("down to item %d\n", event->select.id);
		}
		break;
	case BKEY_SELECT:
		event->type = EXPOACT_SELECT;
		event->select.id = item->id;
		log_debug("select item %d\n", event->select.id);
		break;
	}

	menu_point_to_item(menu, item->id);

	return 0;
}

int scene_menuitem_add(struct scene *scn, uint menu_id, const char *name,
		       uint id, uint key_id, uint text_id, uint preview_id,
		       struct scene_menuitem **itemp)
{
	struct scene_obj_menu *menu;
	struct scene_menuitem *item;
	int ret;

	menu = scene_obj_find(scn, menu_id, SCENEOBJT_MENU);
	if (!menu)
		return log_msg_ret("find", -ENOENT);

	/* Check that the text ID is valid */
	if (!scene_obj_find(scn, text_id, SCENEOBJT_TEXT))
		return log_msg_ret("txt", -EINVAL);

	item = calloc(1, sizeof(struct scene_obj_menu));
	if (!item)
		return log_msg_ret("item", -ENOMEM);
	item->name = strdup(name);
	if (!item->name) {
		free(item);
		return log_msg_ret("name", -ENOMEM);
	}

	item->id = resolve_id(scn->expo, id);
	item->key_id = key_id;
	item->text_id = text_id;
	item->preview_id = preview_id;
	list_add_tail(&item->sibling, &menu->item_head);

	ret = scene_menu_arrange(scn, menu);
	if (ret)
		return log_msg_ret("pos", ret);

	if (itemp)
		*itemp = item;

	return item->id;
}

int scene_menu_set_title(struct scene *scn, uint id, uint title_id)
{
	struct scene_obj_menu *menu;
	struct scene_obj_txt *txt;

	menu = scene_obj_find(scn, id, SCENEOBJT_MENU);
	if (!menu)
		return log_msg_ret("menu", -ENOENT);

	/* Check that the ID is valid */
	if (title_id) {
		txt = scene_obj_find(scn, title_id, SCENEOBJT_TEXT);
		if (!txt)
			return log_msg_ret("txt", -EINVAL);
	}

	menu->title_id = title_id;

	return 0;
}

int scene_menu_set_pointer(struct scene *scn, uint id, uint pointer_id)
{
	struct scene_obj_menu *menu;
	struct scene_obj *obj;

	menu = scene_obj_find(scn, id, SCENEOBJT_MENU);
	if (!menu)
		return log_msg_ret("menu", -ENOENT);

	/* Check that the ID is valid */
	if (pointer_id) {
		obj = scene_obj_find(scn, pointer_id, SCENEOBJT_NONE);
		if (!obj)
			return log_msg_ret("obj", -EINVAL);
	}

	menu->pointer_id = pointer_id;

	return 0;
}
