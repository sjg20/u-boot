// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of a menu in a scene
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EXPO

#include <common.h>
#include <expo.h>
#include <menu.h>
#include <video_console.h>
#include "scene_internal.h"

int scene_textline(struct scene *scn, const char *name, uint id, uint max_chars,
		   struct scene_obj_textline **tlinep)
{
	struct scene_obj_textline *tline;
	int ret, i;
	char *buf;

	ret = scene_obj_add(scn, name, id, SCENEOBJT_TEXTLINE,
			    sizeof(struct scene_obj_textline),
			    (struct scene_obj **)&tline);
	if (ret < 0)
		return log_msg_ret("obj", -ENOMEM);
	abuf_init(&tline->buf);
	if (!abuf_realloc(&tline->buf, max_chars + 1))
		return log_msg_ret("buf", -ENOMEM);
	buf = abuf_data(&tline->buf);
	for (i = 0; i < max_chars; i++) {
		buf[i] = 'a' + i;
	}
	buf[i] = '\0';

	if (tlinep)
		*tlinep = tline;

	return tline->obj.id;
}

void scene_textline_calc_bbox(struct scene_obj_textline *tline,
			      struct vidconsole_bbox *bbox,
			      struct vidconsole_bbox *edit_bbox)
{
	const struct expo_theme *theme = &tline->obj.scene->expo->theme;

	bbox->valid = false;
	scene_bbox_union(tline->obj.scene, tline->label_id, 0, bbox);
	scene_bbox_union(tline->obj.scene, tline->edit_id, 0, bbox);

	edit_bbox->valid = false;
	scene_bbox_union(tline->obj.scene, tline->edit_id, theme->menu_inset,
			 edit_bbox);
}

int scene_textline_calc_dims(struct scene_obj_textline *tline)
{
	struct vidconsole_bbox bbox, edit_bbox;

	scene_textline_calc_bbox(tline, &bbox, &edit_bbox);

	if (bbox.valid) {
		tline->obj.dim.w = bbox.x1 - bbox.x0;
		tline->obj.dim.h = bbox.y1 - bbox.y0;
	}

	return 0;
}

int scene_textline_arrange(struct scene *scn, struct scene_obj_textline *tline)
{
	const bool open = tline->obj.flags & SCENEOF_OPEN;
	bool point;
	int x, y;
	int ret;

	x = tline->obj.dim.x;
	y = tline->obj.dim.y;
	if (tline->label_id) {
		ret = scene_obj_set_pos(scn, tline->label_id, tline->obj.dim.x,
					y);
		if (ret < 0)
			return log_msg_ret("tit", ret);

		ret = scene_obj_set_pos(scn, tline->edit_id,
					tline->obj.dim.x + 200, y);
		if (ret < 0)
			return log_msg_ret("tit", ret);

		ret = scene_obj_get_hw(scn, tline->label_id, NULL);
		if (ret < 0)
			return log_msg_ret("hei", ret);

		y += ret * 2;
	}

	point = scn->highlight_id == tline->obj.id;
	point &= !open;
	scene_obj_flag_clrset(scn, tline->edit_id, SCENEOF_POINT,
			      point ? SCENEOF_POINT : 0);

	return 0;
}

int scene_textline_send_key(struct scene *scn, struct scene_obj_textline *tline,
			    int key, struct expo_action *event)
{
	const bool open = tline->obj.flags & SCENEOF_OPEN;

	switch (key) {
	case BKEY_QUIT:
		if (open) {
			event->type = EXPOACT_CLOSE;
			event->select.id = tline->obj.id;
		} else {
			event->type = EXPOACT_QUIT;
			log_debug("menu quit\n");
		}
		break;
	}

	return 0;
}

int scene_textline_render_deps(struct scene *scn,
			       struct scene_obj_textline *tline)
{
	scene_render_deps(scn, tline->label_id);
	scene_render_deps(scn, tline->edit_id);

	return 0;
}
