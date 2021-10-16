/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootstd_h
#define __bootstd_h

/**
 * struct bootstd_priv - priv data for the bootstd driver
 *
 * @prefixes: NULL-terminated list of prefixes to use for bootflow filenames,
 *	e.g. "/", "/boot/"; NULL if none
 * @order: Order to use for bootdevs, e.g. "mmc2", "mmc1"; NULL if none
 */
struct bootstd_priv {
	const char **prefixes;
	const char **order;
};

/**
 * bootstd_get_order() - Get the boot-order list
 *
 * This reads the boot order, e.g. {"mmc0", "mmc2", NULL}
 *
 * The list is alloced by the bootstd driver so should not be freed.
 *
 * @return list of string points, terminated by NULL; or NULL if no boot order
 */
const char **bootstd_get_order(struct udevice *dev);

#endif
