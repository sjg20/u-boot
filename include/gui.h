/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _GUI_H
#define _GUI_H

struct mouse_event;

struct gui_ops {
	int (*get_context)(struct udevice *dev, void **contextp);

	int (*start_poll)(struct udevice *dev);
	int (*process_mouse_event)(struct udevice *dev,
				   const struct mouse_event *evt);
	int (*input_done)(struct udevice *dev);
	int (*render)(struct udevice *dev);
	int (*end_poll)(struct udevice *dev);
};

#define gui_get_ops(dev)	((struct gui_ops *)(dev)->driver->ops)

int gui_get_context(struct udevice *dev, void **contextp);

int gui_start_poll(struct udevice *dev);
int gui_process_mouse_event(struct udevice *dev, const struct mouse_event *evt);
int gui_input_done(struct udevice *dev);
int gui_render(struct udevice *dev);
int gui_end_poll(struct udevice *dev);

#endif
