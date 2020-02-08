// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <feature.h>
#include <gui.h>
#include <os.h>
#include <version.h>
#include <video.h>

#include "nuklear/gui.h"
#include "nuklear/nuklear.h"

#define STBI_NO_STDIO
#include <nuklear/stb_image.h>

/**
 * enum diag_state - Current state of diagnostics
 *
 * @STATE_INACTIVE: Waiting to select a diagnostics routine
 * @STATE_START: Selected a routine and waiting for it to start
 * @STATE_RUN: Diagnostics routine is running
 * @STATE_SUCCESS: Routine passed
 * @STATE_FAILURE: Routine failed
 * @STATE_ABORT: Aborted by user
 */
enum diag_state {
	STATE_INACTIVE,
	STATE_START,
	STATE_RUN,
	STATE_SUCCESS,
	STATE_FAILURE,
	STATE_ABORT,

	STATE_COUNT,
};

static char *const state_str[] = {
	[STATE_INACTIVE]	= "Inactive",
	[STATE_START]		= "Start",
	[STATE_RUN]		= "Run",
	[STATE_SUCCESS]		= "Passed",
	[STATE_FAILURE]		= "Failed",
};

/**
 * struct feature_diag_priv - private info for diagnostics
 */
struct feature_diag_priv {
	enum diag_state state;
	int routine;
	int errcode;
	uint step;
	uint total_steps;
	void *buf;
	uint buf_size;
	struct nk_context *ctx;
	struct udevice *vid;
	struct nuklear_info *info;
	struct nk_image image_chromeos;
	struct nk_image image_careena;

	union {
		struct {
			uint disk_block;
		} disk_read;
	};
};

typedef int (*diag_handler)(struct feature_diag_priv *priv);

struct diag_info {
	const char *name;
	diag_handler handler;
};

static int diag_disk_read(struct feature_diag_priv *priv)
{
	struct nk_context *ctx = priv->ctx;
	char addr_str[12];

	switch (priv->state) {
	case STATE_START:
		priv->total_steps = 100;
		priv->state = STATE_RUN;
		priv->disk_read.disk_block = 0;
		break;
	case STATE_RUN:
		nk_layout_row_dynamic(ctx, 35, 2);
		nk_label(ctx, "Disk block", NK_TEXT_LEFT);
		sprintf(addr_str, "%08x", priv->disk_read.disk_block);
		nk_label(ctx, addr_str, NK_TEXT_LEFT);

		priv->disk_read.disk_block += 0x1000;
		if (++priv->step == priv->total_steps)
			priv->state = STATE_SUCCESS;
		break;
	default:
		break;
	}

	return 0;
}

static int diag_memory(struct feature_diag_priv *priv)
{
	return 0;
}

static int diag_display(struct feature_diag_priv *priv)
{
	return 0;
}

static int diag_keyboard(struct feature_diag_priv *priv)
{
	return 0;
}

static int diag_audio(struct feature_diag_priv *priv)
{
	return 0;
}

static struct diag_info diag_list[] = {
	{ .name = "Disk read", .handler = diag_disk_read },
	{ .name = "Memory", .handler = diag_memory },
	{ .name = "Display", .handler = diag_display },
	{ .name = "Keyboard", .handler = diag_keyboard },
	{ .name = "Audio", .handler = diag_audio },
};

static void skip(struct nk_context *ctx, int count)
{
	int i;

	for (i = 0; i < count; i++)
		nk_label(ctx, "", NK_TEXT_LEFT);
}

static void add_field_value(struct nk_context *ctx,
			    struct feature_diag_priv *priv, const char *field,
			    const char *value)
{
	nk_style_set_font(ctx, priv->info->font_bold);
	nk_label(ctx, field, NK_TEXT_LEFT);
	nk_label(ctx, "", NK_TEXT_LEFT);
	nk_style_set_font(ctx, priv->info->font_default);
	nk_label(ctx, value, NK_TEXT_LEFT);
	skip(ctx, 1);
}

static void process_diag(struct feature_diag_priv *priv, struct nk_context *ctx)
{
	struct diag_info *diag;
	int selected = -1;
	nk_size percent;
	int ret;
	int i;

	nk_layout_row_begin(ctx, NK_STATIC, 80, 2);

	nk_style_set_font(ctx, priv->info->font_large);
	nk_layout_row_push(ctx, priv->image_chromeos.h);
        nk_image(ctx, priv->image_chromeos);
	nk_layout_row_push(ctx, 1200);
	nk_label(ctx, "Chrome OS Pre-boot Diagnostics", NK_TEXT_LEFT);
	nk_style_set_font(ctx, priv->info->font_default);

	nk_layout_row_dynamic(ctx, 35, 1);

	nk_layout_row_dynamic(ctx, 170, 2);

	nk_style_set_font(ctx, priv->info->font_bold);
	if (nk_group_begin(ctx, "Chromebook Information",
		           NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR |
		           NK_WINDOW_TITLE)) {
		nk_layout_row_dynamic(ctx, 20, 4);
		add_field_value(ctx, priv, "Product", "Chrome OS sandbox");
		add_field_value(ctx, priv, "Firmware version",
				U_BOOT_VERSION_STRING);
		add_field_value(ctx, priv, "Serial number", "1A32X102394");
		add_field_value(ctx, priv, "Diagnostics version", "0.01poc");
		nk_group_end(ctx);
	}

	nk_layout_row_dynamic(ctx, 400, 2);
	if (nk_group_begin(ctx, "Available routines",
		           NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR |
		           NK_WINDOW_TITLE)) {
		nk_layout_row_dynamic(ctx, 35, 3);
		if (priv->state == STATE_INACTIVE) {
			for (i = 0; i < ARRAY_SIZE(diag_list); i++) {
				struct diag_info *diag = &diag_list[i];

				if (nk_button_label(ctx, diag->name))
					selected = i;
				skip(ctx, 2);
			}

			if (selected != -1) {
				priv->state = STATE_START;
				priv->routine = selected;
				priv->step = 0;
			}
		} else {
			diag = &diag_list[priv->routine];

			nk_label(ctx, diag->name, NK_TEXT_CENTERED);
			percent = priv->total_steps ?
				priv->step * 100 / priv->total_steps :
				100;
			nk_progress(ctx, &percent, 100, 0);

			nk_layout_row_dynamic(ctx, 35, 2);
			nk_label(ctx, "Status", NK_TEXT_LEFT);
			nk_label(ctx, state_str[priv->state], NK_TEXT_LEFT);
			if (priv->state == STATE_SUCCESS ||
			    priv->state == STATE_FAILURE ||
			    priv->state == STATE_ABORT) {
				if (nk_button_label(ctx, "OK"))
					priv->state = STATE_INACTIVE;
			} else if (priv->state == STATE_RUN) {
				if (nk_button_label(ctx, "Abort"))
					priv->state = STATE_ABORT;
			}

			priv->ctx = ctx;
			ret = diag->handler(priv);
			if (ret) {
				printf("Error %d\n", ret);
				priv->state = STATE_FAILURE;
				priv->errcode = ret;
			}
		}
		nk_group_end(ctx);
	}
	nk_layout_row_push(ctx, priv->image_careena.h);
        nk_image(ctx, priv->image_careena);
}

static int diag_render(struct udevice *dev)
{
	struct feature_diag_priv *priv = dev_get_priv(dev);
	struct video_priv *upriv = dev_get_uclass_priv(priv->vid);
	struct udevice *gui = feature_get_gui(dev);
	struct nuklear_info *info;
	struct nk_context *ctx;
	int ret;

	if (!gui)
		return log_msg_ret("gui", -ENXIO);
	ret = gui_get_context(gui, (void **)&info);
	if (ret)
		return log_msg_ret("ctx", ret);
	priv->info = info;

	ctx = info->ctx;
	if (nk_begin(ctx, "Diagnostics", nk_rect(0, 0, upriv->xsize,
		     upriv->ysize), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR |
		     NK_WINDOW_MOVABLE)) {
		process_diag(priv, ctx);
		nk_end(ctx);
	}

	return 0;
}

const struct feature_ops feature_diag_ops = {
	.render		= diag_render,
};

static int feature_diag_probe(struct udevice *dev)
{
	struct feature_diag_priv *priv = dev_get_priv(dev);
	struct udevice *gui = feature_get_gui(dev);
	int ret;

	priv->state = STATE_INACTIVE;
	priv->buf = 0;
	priv->buf_size = 32 << 20;
	priv->vid = feature_get_video(dev);

	ret = gui_nuklear_add_image(gui, "chrome_col80", &priv->image_chromeos);
	if (ret)
		return log_msg_ret("chrome", ret);

	ret = gui_nuklear_add_image(gui, "careena", &priv->image_careena);
	if (ret)
		return log_msg_ret("chrome", ret);

	return 0;
}

static const struct udevice_id feature_diag_ids[] = {
	{ .compatible = "feature,diagnostics" },
	{ }
};

U_BOOT_DRIVER(feature_diag) = {
	.name	= "feature_diag",
	.id	= UCLASS_FEATURE,
	.of_match = feature_diag_ids,
	.priv_auto_alloc_size = sizeof(struct feature_diag_priv),
	.ops	= &feature_diag_ops,
	.probe	= feature_diag_probe,
};
