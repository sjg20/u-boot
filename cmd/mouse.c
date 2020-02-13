// SPDX-License-Identifier: GPL-2.0+
/*
 * Mouse testing
 *
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <mouse.h>

static int do_mouse_dump(cmd_tbl_t *cmdtp, int flag, int argc,
			 char *const argv[])
{
	struct udevice *dev;
	bool running;
	int count;
	int ret;

	ret = uclass_first_device_err(UCLASS_MOUSE, &dev);
	if (ret) {
		printf("Mouse not found (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	for (running = true, count = 0; running;) {
		struct mouse_event evt;

		ret = mouse_get_event(dev, &evt);
		if (!ret) {
			switch (evt.type) {
			case MOUSE_EV_BUTTON: {
				struct mouse_button *but = &evt.button;

				printf("button: button==%d, press=%d, clicks=%d, X=%d, Y=%d\n",
				       but->button, but->press_state,
				       but->clicks, but->x, but->y);
				break;
			}
			case MOUSE_EV_MOTION: {
				struct mouse_motion *motion = &evt.motion;
				printf("motion: Xrel=%d, Yrel=%d, X=%d, Y=%d, but=%d\n",
				       motion->xrel, motion->yrel, motion->x,
				       motion->y, motion->state);
				break;
			}
			case MOUSE_EV_NULL:
				break;
			}
			count++;
		} else if (ret != -EAGAIN) {
			return log_msg_ret("get_event", ret);
		}
		if (ctrlc())
			running = false;
	}
	printf("%d events received\n", count);

	return 0;
}


static char mouse_help_text[] =
	"dump - Dump input from a mouse";

U_BOOT_CMD_WITH_SUBCMDS(mouse, "Mouse input", mouse_help_text,
	U_BOOT_SUBCMD_MKENT(dump, 1, 1, do_mouse_dump));
