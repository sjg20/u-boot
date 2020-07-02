/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Structures for inclusion in global_data
 *
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __DM_TINY_STRUCT_H
#define __DM_TINY_STRUCT_H

/* A struct tinydev * stored as an index into the device linker-list */
typedef u8 tinydev_idx_t;

/* enum dm_data_t - Types of data that can be attached to devices */
enum dm_data_t {
	DEVDATAT_PLAT,
	DEVDATAT_PARENT_PLAT,
	DEVDATAT_UC_PLAT,

	DEVDATAT_PRIV,
	DEVDATAT_PARENT_PRIV,
	DEVDATAT_UC_PRIV,
};

struct tinydev_data {
	u8 type;
#ifdef TIMYDEV_SHRINK_DATA
	tinydev_idx_t tdev_idx;
	ushort ofs;
#else
	struct tinydev *tdev;
	void *ptr;
#endif
};

struct tinydev_info {
	int data_count;
	struct tinydev_data data[CONFIG_TINYDEV_DATA_MAX_COUNT];
};

#endif
