// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <gui.h>

int gui_get_context(struct udevice *dev, void **contextp)
{
	struct gui_ops *ops = gui_get_ops(dev);
	int ret;

	if (!ops->get_context)
		return -ENOSYS;

	ret = ops->get_context(dev, contextp);
	if (ret)
		return ret;

	return 0;
}

int gui_start_poll(struct udevice *dev)
{
	struct gui_ops *ops = gui_get_ops(dev);
	int ret;

	if (!ops->start_poll)
		return -ENOSYS;

	ret = ops->start_poll(dev);
	if (ret)
		return ret;

	return 0;
}

int gui_render(struct udevice *dev)
{
	struct gui_ops *ops = gui_get_ops(dev);
	int ret;

	if (!ops->render)
		return -ENOSYS;

	ret = ops->render(dev);
	if (ret)
		return ret;

	return 0;
}

int gui_process_mouse_event(struct udevice *dev, const struct mouse_event *evt)
{
	struct gui_ops *ops = gui_get_ops(dev);
	int ret;

	if (!ops->process_mouse_event)
		return -ENOSYS;

	ret = ops->process_mouse_event(dev, evt);
	if (ret)
		return ret;

	return 0;
}

int gui_input_done(struct udevice *dev)
{
	struct gui_ops *ops = gui_get_ops(dev);
	int ret;

	if (!ops->input_done)
		return -ENOSYS;

	ret = ops->input_done(dev);
	if (ret)
		return ret;

	return 0;
}

int gui_end_poll(struct udevice *dev)
{
	struct gui_ops *ops = gui_get_ops(dev);
	int ret;

	if (!ops->end_poll)
		return -ENOSYS;

	ret = ops->end_poll(dev);
	if (ret)
		return ret;

	return 0;
}

UCLASS_DRIVER(gui) = {
	.id		= UCLASS_GUI,
	.name		= "gui",
};
