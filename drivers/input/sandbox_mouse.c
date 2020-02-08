// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <mouse.h>
#include <asm/sdl.h>

static int mouse_sandbox_get_event(struct udevice *dev,
				   struct mouse_event *event)
{
	int ret;

	ret = sandbox_sdl_get_mouse_event(event);

	return ret;
}

const struct mouse_ops mouse_sandbox_ops = {
	.get_event	= mouse_sandbox_get_event,
};

static const struct udevice_id mouse_sandbox_ids[] = {
	{ .compatible = "sandbox,mouse" },
	{ }
};

U_BOOT_DRIVER(mouse_sandbox) = {
	.name	= "mouse_sandbox",
	.id	= UCLASS_MOUSE,
	.of_match = mouse_sandbox_ids,
	.ops	= &mouse_sandbox_ops,
};
