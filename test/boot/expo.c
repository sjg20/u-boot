// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <expo.h>
#include <video.h>
#include <linux/input.h>
#include <test/suites.h>
#include <test/ut.h>
#include "bootstd_common.h"

enum {
	SCENE1		= 7,
	OBJ_LOGO,
	OBJ_TEXT,
	OBJ_TEXT2,
	OBJ_MENU,
	OBJ_MENU_TITLE,

	ITEM1,
	ITEM2,
	ITEM1_TEXT,
	ITEM1_KEY,
	ITEM1_PREVIEW,
	ITEM2_TEXT,
	ITEM2_KEY,
	ITEM2_PREVIEW,
	CUR_ITEM_TEXT,
};

#define BAD_POINTER	((void *)1)

#define EXPO_NAME	"my menus"
#define SCENE_NAME1	"main"
#define SCENE_NAME2	"second"
#define SCENE_TITLE	"Main Menu"
#define LOGO_NAME	"logo"

/* Check base expo support */
static int expo_base(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct expo *exp;
	ulong start_mem;
	char name[100];
	int i;

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));

	start_mem = ut_check_free();

	exp = NULL;
	strcpy(name, EXPO_NAME);
	ut_assertok(expo_new(name, &exp));
	*name = '\0';
	ut_assertnonnull(exp);
	ut_asserteq(0, exp->scene_id);
	ut_asserteq(0, exp->next_id);

	/* Make sure the name was allocated */
	ut_assertnonnull(exp->name);
	ut_asserteq_str(EXPO_NAME, exp->name);

	ut_assertok(expo_set_display(exp, dev));
	expo_destroy(exp);
	ut_assertok(ut_check_delta(start_mem));

	/* test handling out-of-memory conditions */
	for (i = 0; i < 2; i++) {
		struct expo *exp2;

		malloc_enable_testing(i);
		exp2 = BAD_POINTER;
		ut_asserteq(-ENOMEM, expo_new(EXPO_NAME, &exp2));
		ut_asserteq_ptr(BAD_POINTER, exp2);
		malloc_disable_testing();
	}

	return 0;
}
BOOTSTD_TEST(expo_base, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check creating a scene */
static int expo_scene(struct unit_test_state *uts)
{
	struct scene *scn;
	struct expo *exp;
	ulong start_mem;
	char name[100];
	int id;

	start_mem = ut_check_free();

	ut_assertok(expo_new(EXPO_NAME, &exp));

	scn = NULL;
	ut_asserteq(0, exp->next_id);
	strcpy(name, SCENE_NAME1);
	id = scene_new(exp, name, SCENE1, &scn);
	*name = '\0';
	ut_assertnonnull(scn);
	ut_asserteq(SCENE1, id);
	ut_asserteq(SCENE1 + 1, exp->next_id);
	ut_asserteq_ptr(exp, scn->expo);

	/* Make sure the name was allocated */
	ut_assertnonnull(scn->name);
	ut_asserteq_str(SCENE_NAME1, scn->name);

	/* Set the title */
	strcpy(name, SCENE_TITLE);
	ut_assertok(scene_title_set(scn, name));
	*name = '\0';
	ut_assertnonnull(scn->title);
	ut_asserteq_str(SCENE_TITLE, scn->title);

	/* Use an allocated ID */
	scn = NULL;
	id = scene_new(exp, SCENE_NAME2, 0, &scn);
	ut_assertnonnull(scn);
	ut_asserteq(SCENE1 + 1, id);
	ut_asserteq(SCENE1 + 2, exp->next_id);
	ut_asserteq_ptr(exp, scn->expo);

	ut_asserteq_str(SCENE_NAME2, scn->name);

	expo_destroy(exp);

	ut_assertok(ut_check_delta(start_mem));

	return 0;
}
BOOTSTD_TEST(expo_scene, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check creating a scene with objects */
static int expo_object(struct unit_test_state *uts)
{
	struct scene_obj_img *img;
	struct scene_obj_txt *txt;
	struct scene *scn;
	struct expo *exp;
	ulong start_mem;
	char name[100];
	char *data;
	int id;

	start_mem = ut_check_free();

	ut_assertok(expo_new(EXPO_NAME, &exp));
	id = scene_new(exp, SCENE_NAME1, SCENE1, &scn);
	ut_assert(id > 0);

	ut_asserteq(0, scene_obj_count(scn));

	data = NULL;
	strcpy(name, LOGO_NAME);
	id = scene_img_add(scn, name, OBJ_LOGO, data, &img);
	ut_assert(id > 0);
	*name = '\0';
	ut_assertnonnull(img);
	ut_asserteq(OBJ_LOGO, id);
	ut_asserteq(OBJ_LOGO + 1, exp->next_id);
	ut_asserteq_ptr(scn, img->obj.scene);
	ut_asserteq(SCENEOBJT_IMAGE, img->obj.type);

	ut_asserteq_ptr(data, img->data);

	/* Make sure the name was allocated */
	ut_assertnonnull(scn->name);
	ut_asserteq_str(SCENE_NAME1, scn->name);

	ut_asserteq(1, scene_obj_count(scn));

	id = scene_txt_add(scn, "text", OBJ_TEXT, "my string", &txt);
	ut_assert(id > 0);
	ut_assertnonnull(txt);
	ut_asserteq(OBJ_TEXT, id);
	ut_asserteq(SCENEOBJT_TEXT, txt->obj.type);
	ut_asserteq(2, scene_obj_count(scn));

	/* Check passing NULL as the final parameter */
	id = scene_txt_add(scn, "text2", OBJ_TEXT, "another string", NULL);
	ut_assert(id > 0);
	ut_asserteq(3, scene_obj_count(scn));

	expo_destroy(exp);

	ut_assertok(ut_check_delta(start_mem));

	return 0;
}
BOOTSTD_TEST(expo_object, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check setting object attributes */
static int expo_object_attr(struct unit_test_state *uts)
{
	struct scene_obj_menu *menu;
	struct scene_obj_img *img;
	struct scene_obj_txt *txt;
	struct scene *scn;
	struct expo *exp;
	ulong start_mem;
	char name[100];
	char *data;
	int id;

	start_mem = ut_check_free();

	ut_assertok(expo_new(EXPO_NAME, &exp));
	id = scene_new(exp, SCENE_NAME1, SCENE1, &scn);
	ut_assert(id > 0);

	id = scene_img_add(scn, LOGO_NAME, OBJ_LOGO, data, &img);
	ut_assert(id > 0);

	ut_assertok(scene_obj_set_pos(scn, OBJ_LOGO, 123, 456));
	ut_asserteq(123, img->obj.x);
	ut_asserteq(456, img->obj.y);

	ut_asserteq(-ENOENT, scene_obj_set_pos(scn, OBJ_TEXT2, 0, 0));

	id = scene_txt_add(scn, "text", OBJ_TEXT, "my string", &txt);
	ut_assert(id > 0);

	strcpy(name, "font2");
	ut_assertok(scene_txt_set_font(scn, OBJ_TEXT, name, 42));
	ut_asserteq_ptr(name, txt->font_name);
	ut_asserteq(42, txt->font_size);

	ut_asserteq(-ENOENT, scene_txt_set_font(scn, OBJ_TEXT2, name, 42));

	id = scene_menu_add(scn, "main", OBJ_MENU, &menu);
	ut_assert(id > 0);

	ut_assertok(scene_menu_set_title(scn, OBJ_MENU, OBJ_TEXT));

	ut_asserteq(-ENOENT, scene_menu_set_title(scn, OBJ_TEXT2, OBJ_TEXT));
	ut_asserteq(-EINVAL, scene_menu_set_title(scn, OBJ_MENU, OBJ_TEXT2));

	expo_destroy(exp);

	ut_assertok(ut_check_delta(start_mem));

	return 0;
}
BOOTSTD_TEST(expo_object_attr, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check creating a scene with a menu */
static int expo_object_menu(struct unit_test_state *uts)
{
	struct scene_obj_menu *menu;
	struct scene_menuitem *item;
	int id, txt_id, key_id, pointer_id, preview_id;
	struct scene_obj_txt *ptr, *txt1, *key1, *tit, *prev1;
	struct scene *scn;
	struct expo *exp;
	ulong start_mem;

	start_mem = ut_check_free();

	ut_assertok(expo_new(EXPO_NAME, &exp));
	id = scene_new(exp, SCENE_NAME1, SCENE1, &scn);
	ut_assert(id > 0);

	id = scene_menu_add(scn, "main", OBJ_MENU, &menu);
	ut_assert(id > 0);
	ut_assertnonnull(menu);
	ut_asserteq(OBJ_MENU, id);
	ut_asserteq(SCENEOBJT_MENU, menu->obj.type);
	ut_asserteq(0, menu->title_id);
	ut_asserteq(0, menu->pointer_id);

	ut_assertok(scene_obj_set_pos(scn, OBJ_MENU, 50, 400));
	ut_asserteq(50, menu->obj.x);
	ut_asserteq(400, menu->obj.y);

	id = scene_txt_add(scn, "title", OBJ_MENU_TITLE, "Main Menu", &tit);
	ut_assert(id > 0);
	ut_assertok(scene_menu_set_title(scn, OBJ_MENU, OBJ_MENU_TITLE));
	ut_asserteq(OBJ_MENU_TITLE, menu->title_id);

	pointer_id = scene_txt_add(scn, "cur_item", CUR_ITEM_TEXT, ">", &ptr);
	ut_assert(pointer_id > 0);

	ut_assertok(scene_menu_set_pointer(scn, OBJ_MENU, CUR_ITEM_TEXT));
	ut_asserteq(CUR_ITEM_TEXT, menu->pointer_id);

	txt_id = scene_txt_add(scn, "item1", ITEM1_TEXT, "Lord Melchett",
			       &txt1);
	ut_assert(txt_id > 0);

	key_id = scene_txt_add(scn, "item1-key", ITEM1_KEY, "1", &key1);
	ut_assert(txt_id > 0);

	preview_id = scene_txt_add(scn, "item1-preview", ITEM1_PREVIEW,
				   "(preview1)", &prev1);
	ut_assert(preview_id > 0);

	id = scene_menuitem_add(scn, OBJ_MENU, "linux", ITEM1, ITEM1_KEY,
				ITEM1_TEXT, ITEM1_PREVIEW, &item);
	ut_asserteq(ITEM1, id);
	ut_asserteq(id, item->id);
	ut_asserteq(key_id, item->key_id);
	ut_asserteq(txt_id, item->text_id);
	ut_asserteq(preview_id, item->preview_id);

	/* adding an item should cause the first item to become current */
	ut_asserteq(id, menu->cur_item_id);

	/* the title should be at the top */
	ut_asserteq(menu->obj.x, tit->obj.x);
	ut_asserteq(menu->obj.y, tit->obj.y);

	/* the first item should be next */
	ut_asserteq(menu->obj.x, key1->obj.x);
	ut_asserteq(menu->obj.y + 16, key1->obj.y);

	ut_asserteq(menu->obj.x + 50, ptr->obj.x);
	ut_asserteq(menu->obj.y + 16, ptr->obj.y);

	ut_asserteq(menu->obj.x + 100, txt1->obj.x);
	ut_asserteq(menu->obj.y + 16, txt1->obj.y);

	ut_asserteq(menu->obj.x + 400, prev1->obj.x);
	ut_asserteq(menu->obj.y + 16, prev1->obj.y);

	expo_destroy(exp);

	ut_assertok(ut_check_delta(start_mem));

	return 0;
}
BOOTSTD_TEST(expo_object_menu, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check rendering a scene */
static int expo_render_image(struct unit_test_state *uts)
{
	struct scene_obj_menu *menu;
	struct expo_action act;
	struct udevice *dev;
	struct scene *scn;
	struct expo *exp;
	int id;

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));

	ut_assertok(expo_new(EXPO_NAME, &exp));
	id = scene_new(exp, SCENE_NAME1, SCENE1, &scn);
	ut_assert(id > 0);
	ut_assertok(expo_set_display(exp, dev));

	id = scene_img_add(scn, "img", OBJ_LOGO, video_get_u_boot_logo(), NULL);
	ut_assert(id > 0);
	ut_assertok(scene_obj_set_pos(scn, OBJ_LOGO, 50, 20));

	id = scene_txt_add(scn, "text", OBJ_TEXT, "my string", NULL);
	ut_assert(id > 0);
	ut_assertok(scene_txt_set_font(scn, OBJ_TEXT, "cantoraone_regular",
				       40));
	ut_assertok(scene_obj_set_pos(scn, OBJ_TEXT, 400, 100));

	id = scene_txt_add(scn, "text", OBJ_TEXT2, "another string", NULL);
	ut_assert(id > 0);
	ut_assertok(scene_txt_set_font(scn, OBJ_TEXT2, "nimbus_sans_l_regular",
				       60));
	ut_assertok(scene_obj_set_pos(scn, OBJ_TEXT2, 200, 600));

	id = scene_menu_add(scn, "main", OBJ_MENU, &menu);
	ut_assert(id > 0);

	id = scene_txt_add(scn, "title", OBJ_MENU_TITLE, "Main Menu", NULL);
	ut_assert(id > 0);
	ut_assertok(scene_menu_set_title(scn, OBJ_MENU, OBJ_MENU_TITLE));

	id = scene_txt_add(scn, "cur_item", CUR_ITEM_TEXT, ">", NULL);
	ut_assert(id > 0);
	ut_assertok(scene_menu_set_pointer(scn, OBJ_MENU, CUR_ITEM_TEXT));

	id = scene_txt_add(scn, "item1-preview", ITEM1_PREVIEW, "(preview1)",
			   NULL);
	ut_assert(id > 0);

	id = scene_txt_add(scn, "item1 txt", ITEM1_TEXT, "Lord Melchett", NULL);
	ut_assert(id > 0);
	id = scene_txt_add(scn, "item1-key", ITEM1_KEY, "1", NULL);
	ut_assert(id > 0);
	id = scene_menuitem_add(scn, OBJ_MENU, "item1", ITEM1, ITEM1_KEY,
				ITEM1_TEXT, ITEM1_PREVIEW, NULL);
	ut_assert(id > 0);

	id = scene_txt_add(scn, "item2 txt", ITEM2_TEXT, "Lord Percy", NULL);
	ut_assert(id > 0);
	id = scene_txt_add(scn, "item2-key", ITEM2_KEY, "2", NULL);
	ut_assert(id > 0);
	id = scene_txt_add(scn, "item2-preview", ITEM2_PREVIEW, "(preview2)",
			   NULL);
	ut_assert(id > 0);

	id = scene_menuitem_add(scn, OBJ_MENU, "item2", ITEM2, ITEM2_KEY,
				ITEM2_TEXT, ITEM2_PREVIEW, NULL);
	ut_assert(id > 0);

	ut_assertok(scene_obj_set_pos(scn, OBJ_MENU, 50, 400));

	/* render without a scene */
	ut_asserteq(-ECHILD, expo_render(exp));

	/* render it */
	expo_set_scene_id(exp, SCENE1);
	ut_assertok(expo_render(exp));

	/* move down */
	ut_assertok(expo_send_key(exp, KEY_DOWN));

	ut_assertok(expo_action_get(exp, &act));

	ut_asserteq(EXPOACT_POINT, act.type);
	ut_asserteq(ITEM2, act.select.id);
	ut_assertok(expo_render(exp));

	/* select it */
	ut_assertok(expo_send_key(exp, KEY_ENTER));

	ut_assertok(expo_action_get(exp, &act));
	ut_asserteq(EXPOACT_SELECT, act.type);
	ut_asserteq(ITEM2, act.select.id);

	ut_asserteq(-EAGAIN, expo_action_get(exp, &act));

	ut_assertok(expo_render(exp));

	expo_destroy(exp);

	return 0;
}
BOOTSTD_TEST(expo_render_image, UT_TESTF_DM | UT_TESTF_SCAN_FDT);
