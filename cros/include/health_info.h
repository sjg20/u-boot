/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __CROS_HEALTH_INFO_H__
#define __CROS_HEALTH_INFO_H__

#include <cros/storage_info.h>

static inline int clz(u32 x) { return x ? __builtin_clz(x) : sizeof(x) * 8; }

// Append the stringified health_info to string buf and return the pointer of
// the next available address of buf.
char *stringify_health_info(char *buf, const char *end, const HealthInfo *info);

// Append the health info of all devices to string buf and return the pointer of
// the next available address of buf.
char *dump_all_health_info(char *buf, const char *end);

#endif
