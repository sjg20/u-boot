/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Events provide a general-purpose way to react to / subscribe to changes
 * within U-Boot
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __event_h
#define __event_h

/**
 * enum event_t - Types of events supported by U-Boot
 *
 * @EVT_DM_PRE_PROBE: Device is about to be probed
 */
enum event_t {
	EVT_NONE,
	EVT_TEST,

	/* Events related to driver model */
	EVT_DM_PRE_PROBE,
	EVT_DM_POST_PROBE,
	EVT_DM_PRE_REMOVE,
	EVT_DM_POST_REMOVE,

	EVT_COUNT
};

union event_data {
	/**
	 * struct event_data_test  - test data
	 *
	 * @signal: A value to update the state with
	 */
	struct event_data_test {
		int signal;
	} test;

	/**
	 * struct event_dm - driver model event
	 *
	 * @dev: Device this event relates to
	 */
	struct event_dm {
		struct udevice *dev;
	} dm;
};

/**
 * struct event - an event that can be sent and received
 *
 * @type: Event type
 * @data: Data for this particular event
 */
struct event {
	enum event_t type;
	union event_data data;
};

/** Function type for event handlers */
typedef int (*event_handler_t)(void *ctx, struct event *event);

/**
 * event_register - register a new spy
 *
 * @id: Spy ID
 * @type: Event type to subscribe to
 * @func: Function to call when the event is sent
 * @ctx: Context to pass to the function
 * @return 0 if OK, -ve on erropr
 */
int event_register(const char *id, enum event_t type, event_handler_t func,
		   void *ctx);

/**
 * event_notify() - notify spies about an event
 *
 * It is possible to pass in union event_data here but that may not be
 * convenient if the data is elsewhere, or is one of the members of the union.
 * So this uses a void * for @data, with a separate @size.
 *
 * @type: Event type
 * @data: Event data to be sent (e.g. union_event_data)
 * @size: Size of data in bytes
 */
int event_notify(enum event_t type, void *data, int size);

#if CONFIG_IS_ENABLED(EVENT)
int event_uninit(void);
int event_init(void);
#else
static inline int event_uninit(void)
{
	return 0;
}

static inline int event_init(void)
{
	return 0;
}
#endif

#endif
