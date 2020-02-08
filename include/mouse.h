/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Mouse/trackpad/touchscreen input uclass
 *
 * Copyright 2020 Google LLC
 */

#ifndef _MOUSE_H
#define _MOUSE_H

enum mouse_ev_t {
	MOUSE_EV_NULL,
	MOUSE_EV_MOTION,
	MOUSE_EV_BUTTON,
};

enum mouse_state_t {
	BUTTON_LEFT		= 1 << 0,
	BUTTON_MIDDLE		= 1 << 1,
	BUTTON_RIGHT		= 1 << 2,
	BUTTON_SCROLL_PLUS	= 1 << 3,
	BUTTON_SCROLL_MINUS	= 1 << 4,
};

enum mouse_press_state_t {
	BUTTON_RELEASED		= 0,
	BUTTON_PRESSED,
};

/**
 * struct mouse_event - information about a mouse event
 *
 * @type: Mouse event ype
 */
struct mouse_event {
	enum mouse_ev_t type;
	union {
		/**
		 * @state: Mouse state (enum mouse_state_t bitmask)
		 * @x: X position of mouse
		 * @y: Y position of mouse
		 * @xrel: Relative motion in X direction
		 * @yrel: Relative motion in Y direction
		 */
		struct mouse_motion {
			unsigned char state;
			unsigned short x;
			unsigned short y;
			short xrel;
			short yrel;
		} motion;

		/**
		 * @button: Button number that was pressed/released (BUTTON_...)
		 * @state: BUTTON_PRESSED / BUTTON_RELEASED
		 * @clicks: number of clicks (normally 1; 2 = double-click)
		 * @x: X position of mouse
		 * @y: Y position of mouse
		 */
		struct mouse_button {
			unsigned char button;
			unsigned char press_state;
			unsigned char clicks;
			unsigned short x;
			unsigned short y;
		} button;
	};
};

struct mouse_ops {
	int (*get_event)(struct udevice *dev, struct mouse_event *event);
};

#define mouse_get_ops(dev)	((struct mouse_ops *)(dev)->driver->ops)

int mouse_get_event(struct udevice *dev, struct mouse_event *event);

#endif
