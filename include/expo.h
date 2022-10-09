/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __SCENE_H
#define __SCENE_H

#include <linux/list.h>

struct udevice;

enum expoact_type {
	EXPOACT_NONE,		/* no action */
	EXPOACT_POINT,		/* menu item was highlighted */
	EXPOACT_SELECT,		/* menu item was selected */
	EXPOACT_QUIT,		/* request to exit the menu */
};

struct expo_action {
	enum expoact_type type;
	union {
		struct {
			int id;
		} select;
	};
};

/**
 * struct expo - information about an expo
 *
 * A group of scenes which can be presented to the user, typically to obtain
 * input or to make a selection.
 *
 * @name: Name of the expo
 * @display: Display to use
 * @scene_id: Current scene ID (0 if none)
 * @next_id: Next ID number to use
 * @action: Action selected by user
 * @priv: Private data for the owner
 * @scene_head: List of scenes
 */
struct expo {
	char *name;
	struct udevice *display;
	uint scene_id;
	uint next_id;
	struct expo_action action;
	void *priv;
	struct list_head scene_head;
};

/**
 * struct scene - information about a scene in an expo
 *
 * A collection of text/image/menu items in an expo
 *
 * @expo: Expo this scene is part of
 * @name: Name of the scene
 * @id: ID number of the scene
 * @title: Title of the scene
 * @sibling: Node to link this scene to its siblings
 * @obj_head: List of objects in the scene
 */
struct scene {
	struct expo *expo;
	char *name;
	uint id;
	char *title;
	struct list_head sibling;
	struct list_head obj_head;
};

/**
 * enum scene_obj_t - type of a scene object
 *
 * @SCENEOBJT_IMAGE: Image data to render
 * @SCENEOBJT_TEXT: Text line to render
 * @SCENEOBJT_MENU: Menu containing items the user can select
 */
enum scene_obj_t {
	SCENEOBJT_NONE		= 0,
	SCENEOBJT_IMAGE,
	SCENEOBJT_TEXT,
	SCENEOBJT_MENU,
};

/**
 * struct scene_obj - information about an object in a scene
 *
 * @scene: Scene that this object relates to
 * @name: Name of the object
 * @id: ID number of the object
 * @type: Type of this object
 * @x: x position
 * @y: y position
 * @hide: true if the object should be hidden
 * @sibling: Node to link this object to its siblings
 */
struct scene_obj {
	struct scene *scene;
	char *name;
	uint id;
	enum scene_obj_t type;
	int x;
	int y;
	bool hide;
	struct list_head sibling;
};

/**
 * struct scene_obj_img - information about an image object in a scene
 *
 * This is a rectangular image which is blitted onto the display
 *
 * @obj: Basic object information
 * @data: Image data in BMP format
 */
struct scene_obj_img {
	struct scene_obj obj;
	char *data;
};

/**
 * struct scene_obj_txt - information about a text object in a scene
 *
 * This is a single-line text object
 *
 * @obj: Basic object information
 * @str: Text string to display
 * @font_name: Name of font
 * @font_size: Nominal size of font in pixels
 */
struct scene_obj_txt {
	struct scene_obj obj;
	const char *str;
	const char *font_name;
	uint font_size;
};

/**
 * struct scene_obj_menu - information about a menu object in a scene
 *
 * A menu has a number of items which can be selected by the user
 *
 * It also has:
 * - a text/image object (@pointer_id) which points to the current item
 *	(@cur_item_id)
 * - a preview object which shows an image related to the current item
 *
 * @obj: Basic object information
 * @title_id: ID of the title text, or 0 if none
 * @cur_item_id: ID of the current menu item, or 0 if none
 * @pointer_id: ID of the object pointing to the current selection
 * @item_head: List of items in the menu
 */
struct scene_obj_menu {
	struct scene_obj obj;
	uint title_id;
	uint cur_item_id;
	uint pointer_id;
	struct list_head item_head;
};

/**
 * struct scene_menuitem - a menu item in a menu
 *
 * A menu item has:
 * - text object holding the name (short) and description (can be longer)
 * - a text object holding the keypress
 *
 * @obj: Basic object information
 * @key_id: ID of text object to use as the keypress to show
 * @name_id: ID of text object to use as the name text
 * @desc_id: ID of text object to use as the description text
 * @preview_id: ID of the preview object, or 0 if none
 * @sibling: Node to link this item to its siblings
 */
struct scene_menuitem {
	char *name;
	uint id;
	uint key_id;
	uint name_id;
	uint desc_id;
	uint preview_id;
	struct list_head sibling;
};

/**
 * expo_new() - create a new expo
 *
 * Allocates a new expo
 *
 * @name: Name of expo (this is allocated by this call)
 * @priv: Private data for the caller
 * @expp: Returns a pointer to the new expo on success
 * Returns: 0 if OK, -ENOMEM if out of memory
 */
int expo_new(const char *name, void *priv, struct expo **expp);

/**
 * expo_destroy() - Destroy an expo and free all its memory
 *
 * @exp: Expo to destroy
 */
void expo_destroy(struct expo *exp);

/**
 * expo_set_display() - set the display to use for a expo
 *
 * @exp: Expo to update
 * @dev: Display to use, NULL to use text mode
 * Returns: 0 (always)
 */
int expo_set_display(struct expo *exp, struct udevice *dev);

/**
 * expo_set_scene_id() - Set the current scene ID
 *
 * @exp: Expo to update
 * @scene_id: New scene ID to use (0 to clear the current scene)
 * Returns 0 if OK, -ENOENT if there is no scene with that ID
 */
int expo_set_scene_id(struct expo *exp, uint scene_id);

/**
 * expo_render() - render the expo on the display
 *
 * Returns 0 if OK, -ECHILD if there is no current scene, -ENOENT if the current
 * scene is is not found, other error if something else goes wrong
 */
int expo_render(struct expo *exp);

#if 0

handle text support - i.e. without a video console

/**
 * expo_theme_set() - set so theme options from a device tree node
 */
int expo_theme_set(struct expo *expo, ofnode node);
#endif

/**
 * scene_new() - create a new scene in a expo
 *
 * The scene is given the ID @id which must be unique across all scenes, objects
 * and items. The scene's @next_id is updated to at least @id + 1
 *
 * @exp: Expo to updatevoid scene_destroy(struct scene *scn)
 * @name: Name to use (this is allocated by this call)
 * @id: ID to use for the new scene (0 to allocate one)
 * @scnp: Returns a pointer to the new scene on success
 * Returns: ID number for the scene, or -ve on error
 */
int scene_new(struct expo *exp, const char *name, uint id, struct scene **scnp);

/**
 * expo_lookup_scene_id() - Look up a scene by ID
 *
 * @exp: Expo to check
 * @scene_id: Scene ID to look up
 * @returns pointer to scene if found, else NULL
 */
struct scene *expo_lookup_scene_id(struct expo *exp, uint scene_id);

/**
 * scene_title_set() - set the scene title
 *
 * @scn: Scene to update
 * @title: Title to set, NULL if none (this is allocated by this call)
 * Returns 0 if OK, -ENOMEM if out of memory
 */
int scene_title_set(struct scene *scn, const char *title);

/**
 * scene_obj_count() - Count the number of objects in a scene
 *
 * @scn: Scene to check
 * Returns: number of objects in the scene, 0 if none
 */
int scene_obj_count(struct scene *scn);

/**
 * scene_img_add() - add a new image to a scene
 *
 * @scn: Scene to update
 * @name: Name to use (this is allocated by this call)
 * @id: ID to use for the new object (0 to allocate one)
 * @data: Pointer to image data
 * @imgp: If non-NULL, returns the new object
 * Returns: ID number for the object, or -ve on error
 */
int scene_img_add(struct scene *scn, const char *name, uint id, char *data,
		  struct scene_obj_img **imgp);

/**
 * scene_txt_add() - add a new text object to a scene
 *
 * @scn: Scene to update
 * @name: Name to use (this is allocated by this call)
 * @id: ID to use for the new object (0 to allocate one)
 * @str: Pointer to text to display
 * @txtp: If non-NULL, returns the new object
 * Returns: ID number for the object, or -ve on error
 */
int scene_txt_add(struct scene *scn, const char *name, uint id, char *str,
		  struct scene_obj_txt **txtp);

/**
 *  scene_menu_add() - create a menu
 *
 * @scn: Scene to update
 * @name: Name to use (this is allocated by this call)
 * @id: ID to use for the new object (0 to allocate one)
 * @menup: If non-NULL, returns the new object
 * Returns: ID number for the object, or -ve on error
 */
int scene_menu_add(struct scene *scn, const char *name, uint id,
		   struct scene_obj_menu **menup);

/**
 * scene_txt_set_font() - Set the font for an object
 *
 * @scn: Scene to update
 * @id: ID of object to update
 * @font_name: Font name to use (not allocated, so must remain valid)
 * @font_size: Font size to use (nominal height in pixels)
 */
int scene_txt_set_font(struct scene *scn, uint id, const char *font_name,
		       uint font_size);

/**
 * scene_obj_set_pos() - Set the postion of an object
 *
 * @scn: Scene to update
 * @id: ID of object to update
 * @x: x position
 * @y: y position
 * @return 0 if OK, -ENOENT if @id is invalid
 */
int scene_obj_set_pos(struct scene *scn, uint id, int x, int y);

/**
 * scene_obj_set_hide() - Set whether an object is hidden
 *
 * @scn: Scene to update
 * @id: ID of object to update
 * @hide: true to hide the object, false to show it
 * @return 0 if OK, -ENOENT if @id is invalid
 */
int scene_obj_set_hide(struct scene *scn, uint id, bool hide);

/**
 * scene_menu_set_title() - Set the title of a menu
 *
 * @scn: Scene to update
 * @id: ID of menu object to update
 * @title_id: ID of text object to use as the title
 * @return 0 if OK, -ENOENT if @id is invalid, -EINVAL if @title_id is invalid
 */
int scene_menu_set_title(struct scene *scn, uint id, uint title_id);

/**
 * scene_menu_set_pointer() - Set the item pointer for a menu
 *
 * This is a visual indicator of the current item, typically a ">" character
 * which sits next to the current item and moves when the user presses the
 * up/down arrow keys
 *
 * @scn: Scene to update
 * @id: ID of menu object to update
 * @cur_item_id: ID of object to use as a pointer to the current item
 * @return 0 if OK, -ENOENT if @id is invalid, -EINVAL if @title_id is invalid
 */
int scene_menu_set_pointer(struct scene *scn, uint id, uint cur_item_id);

/**
 * scene_obj_get_hw() - Get the height of an object in a scene
 *
 * @scn: Scene to check
 * @id: ID of menu object to check
 * @widthp: If non-NULL, returns width of object
 * Returns: Height of object
 */
int scene_obj_get_hw(struct scene *scn, uint id, int *widthp);

/**
 * scene_item_add() - Add an item to a menu
 *
 * @scn: Scene to update
 * @menu_id: ID of menu object to update
 * @name: Name to use (this is allocated by this call)
 * @id: ID to use for the new object (0 to allocate one)
 * @key_id: ID of text object to use as the keypress to show
 * @name_id: ID of text object to use as the name text
 * @desc_id: ID of text object to use as the description text
 * @preview_id: ID of object to use as the preview (text or image)
 * @itemp: If non-NULL, returns the new object
 * Returns: ID number for the item, or -ve on error
 */
int scene_menuitem_add(struct scene *scn, uint menu_id, const char *name,
		       uint id, uint key_id, uint name_id, uint desc_id,
		       uint preview_id, struct scene_menuitem **itemp);

/**
 * scene_arrange() - Arrange the scene to deal with object sizes
 *
 * Updates any menus in the scene so that their objects are in the right place
 *
 * @scn: Scene to arrange
 * Returns: 0 if OK, -ve on error
 */
int scene_arrange(struct scene *scn);

/**
 * expo_send_key() - set a keypress to the expo
 *
 * @exp: Expo to receive the key
 * @key: Key to send (KEYCODE_UP)
 * Returns: 0 if OK, -ECHILD if there is no current scene
 */
int expo_send_key(struct expo *exp, int key);

/**
 * expo_action_get() - read user input from the expo
 *
 * @exp: Expo to check
 * @act: Returns action
 * @return 0 if OK, -EAGAIN if there was no action to return
 */
int expo_action_get(struct expo *exp, struct expo_action *act);

#if 0
int scene_pos_set(struct scene_obj *obj, int x, int y) - set top left position of object

int expo_send_mouse(struct expo *expo, int x, int y, int buttons) - set a mouse movement / click to the expo
#endif

#endif /*__SCENE_H */
