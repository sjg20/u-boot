/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __CROS_MEMORY_UTIL_H__
#define __CROS_MEMORY_UTIL_H__

#include <cros/ranges.h>

/* memory functions (could be in a separate file) */
int memory_range_init_and_get_unused(Ranges *ranges);
int memory_wipe_unused(void);
void memory_mark_used(uint64_t start, uint64_t end);

#endif
