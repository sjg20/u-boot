// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of configuration editor
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <expo.h>
#include <video.h>
#include "scene_internal.h"

int cedit_arange(struct expo *exp, struct video_priv *vpriv, uint scene_id)
{
	struct scene_obj_txt *txt;
	struct scene_obj *obj;
	struct scene *scn;
	int y;

	scn = expo_lookup_scene_id(exp, scene_id);
	if (!scn)
		return log_msg_ret("scn", -ENOENT);

	list_for_each_entry(obj, &scn->obj_head, sibling) {
		printf("%3d %s\n", obj->id, obj->name);
	}

	txt = scene_obj_find_by_name(scn, "prompt");
	if (txt)
		scene_obj_set_pos(scn, txt->obj.id, 0, vpriv->ysize - 50);

	txt = scene_obj_find_by_name(scn, "title");
	if (txt)
		scene_obj_set_pos(scn, txt->obj.id, 200, 10);

	y = 100;
	list_for_each_entry(obj, &scn->obj_head, sibling) {
		if (obj->type == SCENEOBJT_MENU) {
			scene_obj_set_pos(scn, obj->id, 50, y);
			scene_menu_arrange(scn, (struct scene_obj_menu *)obj);
			y += 50;
		}
	}

	return 0;
}
