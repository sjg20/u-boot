// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <feature.h>
#include <gui.h>
#include <mouse.h>
#include <dm/device-internal.h>

struct feature_uc_priv {
	struct udevice *gui;
	struct udevice *mouse;
};

int feature_render(struct udevice *dev)
{
	struct feature_ops *ops = feature_get_ops(dev);
	int ret;

	if (!ops->render)
		return -ENOSYS;

	ret = ops->render(dev);
	if (ret)
		return ret;

	return 0;
}

struct udevice *feature_get_gui(struct udevice *dev)
{
	struct feature_uc_priv *upriv = dev_get_uclass_priv(dev);

	return upriv->gui;
}

struct udevice *feature_get_video(struct udevice *dev)
{
	struct feature_uc_priv *upriv = dev_get_uclass_priv(dev);
	struct udevice *vid = dev_get_parent(upriv->gui);

	return vid;
}

int feature_poll(struct udevice *dev)
{
	struct feature_uc_priv *upriv = dev_get_uclass_priv(dev);
	struct udevice *gui = upriv->gui;
	int ret;

	ret = gui_start_poll(gui);
	if (ret)
		return log_msg_ret("gui", ret);
	if (upriv->mouse) {
		struct mouse_event evt;
		int ret;

		while (ret = mouse_get_event(upriv->mouse, &evt), !ret) {
			gui_process_mouse_event(gui, &evt);
		}
	}
	ret = gui_input_done(gui);
	if (ret)
		return log_msg_ret("input", ret);

	ret = feature_render(dev);
	if (ret)
		return log_msg_ret("render", ret);
	ret = gui_render(gui);
	if (ret)
		return log_msg_ret("render", ret);
	ret = gui_end_poll(gui);
	if (ret)
		return log_msg_ret("end", ret);

	return 0;
}

int feature_start(struct udevice *dev)
{
	int ret;

	ret = device_probe(dev);
	if (ret)
		return log_msg_ret("probe", ret);

	return 0;
}

static int feature_pre_probe(struct udevice *dev)
{
	struct feature_uc_priv *upriv = dev_get_uclass_priv(dev);
	struct ofnode_phandle_args args;
	int ret;

	ret = dev_read_phandle_with_args(dev, "gui", NULL, 0, 0, &args);
	if (ret)
		return log_msg_ret("gui", ret);
	ret = uclass_get_device_by_ofnode(UCLASS_GUI, args.node, &upriv->gui);
	if (ret) {
		pr_err("%s: uclass_get_device_by_ofnode failed: err=%d\n",
		       __func__, ret);
		return ret;
	}
	ret = uclass_first_device(UCLASS_MOUSE, &upriv->mouse);
	if (ret)
		return log_msg_ret("mouse", ret);

	return 0;
}

UCLASS_DRIVER(feature) = {
	.id		= UCLASS_FEATURE,
	.name		= "feature",
	.pre_probe	= feature_pre_probe,
	.per_device_auto_alloc_size	= sizeof(struct feature_uc_priv),
};
