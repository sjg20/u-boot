// SPDX-License-Identifier: GPL-2.0+
/*
 * Events provide a general-purpose way to react to / subscribe to changes
 * within U-Boot
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EVENT

#include <common.h>
#include <event.h>
#include <event_internal.h>
#include <log.h>
#include <malloc.h>
#include <asm/global_data.h>
#include <linux/list.h>

DECLARE_GLOBAL_DATA_PTR;

static void spy_free(struct event_spy *spy)
{
	list_del(&spy->sibling_node);
}

int event_register(const char *id, enum event_t type, event_handler_t func, void *ctx)
{
	struct event_state *state = gd->event_state;
	struct event_spy *spy;

	spy = malloc(sizeof(*spy));
	if (!spy)
		return log_msg_ret("alloc", -ENOMEM);

	spy->id = id;
	spy->type = type;
	spy->func = func;
	spy->ctx = ctx;
	list_add_tail(&spy->sibling_node, &state->spy_head);

	return 0;
}

int event_notify(enum event_t type, void *data, int size)
{
	struct event_state *state = gd->event_state;
	struct event_spy *spy, *next;
	struct event event;

	event.type = type;
	if (size > sizeof(event.data))
		return log_msg_ret("size", -E2BIG);
	memcpy(&event.data, data, size);
	list_for_each_entry_safe(spy, next, &state->spy_head, sibling_node) {
		if (spy->type == type) {
			int ret;

			log_debug("Sending event %x to spy '%s'\n", type,
				  spy->id);
			ret = spy->func(spy->ctx, &event);

			/*
			 * TODO: Handle various return codes to
			 *
			 * - claim an event (no others will see it)
			 * - return an error from the event
			 */
			if (ret)
				return log_msg_ret("spy", ret);
		}
	}

	return 0;
}

int event_uninit(void)
{
	struct event_state *state = gd->event_state;
	struct event_spy *spy, *next;

	if (!state)
		return 0;
	list_for_each_entry_safe(spy, next, &state->spy_head, sibling_node)
		spy_free(spy);

	return 0;
}

int event_init(void)
{
	struct event_state *state;

	state = malloc(sizeof(struct event_state));
	if (!state)
		return log_msg_ret("alloc", -ENOMEM);

	INIT_LIST_HEAD(&state->spy_head);

	gd->event_state = state;

	return 0;
}
