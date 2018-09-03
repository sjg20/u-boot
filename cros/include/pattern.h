/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __CROS_PATTERN_H__
#define __CROS_PATTERN_H__

#include <linux/list.h>

typedef struct Pattern {
	const char *name;

	const uint32_t *data;
	size_t len;

	struct list_head list_node;
} Pattern;

const struct list_head *DiagGetSimpleTestPatterns(void);
const struct list_head *DiagGetTestPatterns(void);

#endif
