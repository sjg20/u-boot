/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __NUKLEAR_GUI_H
#define __NUKLEAR_GUI_H

struct nk_image;

struct nuklear_info {
	struct nk_context *ctx;
	struct nk_user_font *font_default;
	struct nk_user_font *font_bold;
	struct nk_user_font *font_large;
};

int gui_nuklear_add_image(struct udevice *dev, const char *fname,
			  struct nk_image *img);


#endif
