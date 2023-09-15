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
#include "scene_internal.h"

int scene_textline(struct scene *scn, const char *name, uint id,
		   struct scene_obj_textline **tlinep)
{
	struct scene_obj_textline *tline;
	int ret;

	ret = scene_obj_add(scn, name, id, SCENEOBJT_TEXTLINE,
			    sizeof(struct scene_obj_textline),
			    (struct scene_obj **)&tline);
	if (ret < 0)
		return log_msg_ret("obj", -ENOMEM);

	if (tlinep)
		*tlinep = tline;

	return tline->obj.id;
}

int scene_textline_set_title(struct scene *scn, uint id, uint title_id)
{
	struct scene_obj_textline *tline;
	struct scene_obj_txt *txt;

	tline = scene_obj_find(scn, id, SCENEOBJT_TEXTLINE);
	if (!tline)
		return log_msg_ret("tline", -ENOENT);

	/* Check that the ID is valid */
	if (title_id) {
		txt = scene_obj_find(scn, title_id, SCENEOBJT_TEXT);
		if (!txt)
			return log_msg_ret("txt", -EINVAL);
	}

	tline->title_id = title_id;

	return 0;
}

