/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _FEATURE_H
#define _FEATURE_H

struct feature_ops {
	/**
	 * render() - render the UI for this feature
	 *
	 * This is called regularly while the feature is active. This method
	 * should update the GUI based on input received
	 */
	int (*render)(struct udevice *dev);
};

#define feature_get_ops(dev)	((struct feature_ops *)(dev)->driver->ops)

/**
 * feature_render() - render the UI for this feature
 *
 * This is called regularly while the feature is active. This method
 * should update the GUI based on input received
 */
int feature_render(struct udevice *dev);

/**
 * feature_get_gui() - Get the GUI for a feature
 *
 * @dev feature to check
 * @return pointer to GUI, or NULL if none
 */
struct udevice *feature_get_gui(struct udevice *dev);

/**
 * feature_get_video() - Get the video device for a feature
 *
 * @dev feature to check
 * @return pointer to video device, or NULL if none
 */
struct udevice *feature_get_video(struct udevice *dev);

int feature_poll(struct udevice *dev);

#endif
