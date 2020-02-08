// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <mouse.h>

int mouse_get_event(struct udevice *dev, struct mouse_event *evt)
{
	struct mouse_ops *ops = mouse_get_ops(dev);
	int ret;

	if (!ops->get_event)
		return -ENOSYS;

	ret = ops->get_event(dev, evt);
	if (ret)
		return ret;

	return 0;
}

UCLASS_DRIVER(mouse) = {
	.id		= UCLASS_MOUSE,
	.name		= "mouse",
};
