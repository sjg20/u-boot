// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <feature.h>
#include <gui.h>
#include "nuklear/gui.h"
#include "nuklear/nuklear.h"

/*
 * Position of number keys counting from top left, across and then down. Note
 * the '0' in the last row.
 *
 * Key arrangement is:
 *
 *    7 8 9 +
 *    4 5 6 -
 *    1 2 3 *
 *    C 0 = /
 */
static const char numbers[] = "789\0" "456\0" "123\0" "\0000\0\0";

/* Operations keys on the right */
static const char ops[] = "+-*/";

/**
 * struct feature_calc_priv - private info for the calculator
 *
 * @set: true if we have just set an operator, so we can allow it to be changed
 *	to a different operator without messing things up
 * @prev: Previous operator (before '=' is pushed)
 * @op: Last operator that was pressed
 * @val_a: First operand
 * @val_b: Second operand
 * @current: Points to current operation, either val_a or val_b
 *
 */
struct feature_calc_priv {
        bool set;
	int prev;
	int op;
        double val_a;
	double val_b;
	double *current;
};

/* This is the calculator example from Nuklear, but decrypted a little */
static void process_calc(struct feature_calc_priv *priv, struct nk_context *ctx)
{
	bool solve = false;
	char buf[32];
	size_t i;
	int len;

	nk_layout_row_dynamic(ctx, 35, 1);

	/* This should really be %.2f but U-Boot printf() doesn't support it */
	len = snprintf(buf, 256, "%d", (int)*priv->current);
	nk_edit_string(ctx, NK_EDIT_SIMPLE, buf, &len, 255, nk_filter_float);
	buf[len] = 0;
	*priv->current = atof(buf);

	nk_layout_row_dynamic(ctx, 35, 4);

	/* Draw and check the keys one by one, left to right, top to bottom */
	for (i = 0; i < 16; ++i) {
		if (numbers[i]) {
			/* Number key */
			if (nk_button_text(ctx, &numbers[i], 1)) {
				*priv->current = *priv->current * 10.0f +
					numbers[i] - '0';
				priv->set = false;
			}
		} else if ((i % 4) == 3) {
			/* Operator */
			if (nk_button_text(ctx, &ops[i / 4], 1)) {
				if (!priv->set) {
					if (priv->current != &priv->val_b) {
						priv->current = &priv->val_b;
					} else {
						priv->prev = priv->op;
						solve = true;
					}
				}
				priv->op = ops[i / 4];
				priv->set = true;
			}
		} else if (i == 12) {
			if (nk_button_label(ctx, "C")) {
				priv->val_a = 0;
				priv->val_b = 0;
				priv->op = 0;
				priv->current = &priv->val_a;
				priv->set = false;
			}
		} else if (i == 14) {
			if (nk_button_label(ctx, "=")) {
				solve = true;
				priv->prev = priv->op;
				priv->op = 0;
			}
		}
	}
	if (solve) {
		if (priv->prev == '+')
			priv->val_a += priv->val_b;
		if (priv->prev == '-')
			priv->val_a -= priv->val_b;
		if (priv->prev == '*')
			priv->val_a *= priv->val_b;
		if (priv->prev == '/')
			priv->val_a /= priv->val_b;
		priv->current = &priv->val_a;
		if (priv->set)
			priv->current = &priv->val_b;
		priv->val_b = 0;
		priv->set = false;
	}
}

static int calc_render(struct udevice *dev)
{
	struct feature_calc_priv *priv = dev_get_priv(dev);
	struct udevice *gui = feature_get_gui(dev);
	struct nuklear_info *info;
	struct nk_context *ctx;
	int ret;

	if (!gui)
		return log_msg_ret("gui", -ENXIO);
	ret = gui_get_context(gui, (void **)&info);
	if (ret)
		return log_msg_ret("ctx", ret);
	ctx = info->ctx;

	if (nk_begin(ctx, "Calculator", nk_rect(10, 10, 180, 250),
		     NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR |
		     NK_WINDOW_MOVABLE))
		process_calc(priv, ctx);
	nk_end(ctx);

	return 0;
}

const struct feature_ops feature_calc_ops = {
	.render		= calc_render,
};

static int feature_calc_probe(struct udevice *dev)
{
	struct feature_calc_priv *priv = dev_get_priv(dev);

	priv->current = &priv->val_a;

	return 0;
}

static const struct udevice_id feature_calc_ids[] = {
	{ .compatible = "feature,calculator" },
	{ }
};

U_BOOT_DRIVER(feature_calc) = {
	.name	= "feature_calc",
	.id	= UCLASS_FEATURE,
	.of_match = feature_calc_ids,
	.priv_auto_alloc_size = sizeof(struct feature_calc_priv),
	.ops	= &feature_calc_ops,
	.probe	= feature_calc_probe,
};
