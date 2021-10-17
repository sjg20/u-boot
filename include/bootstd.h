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
 * @order: Order to use for bootdevs (or NULL if none), with each item being a
 * 	bootdev label, e.g. "mmc2", "mmc1";
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
 * The list is alloced by the bootstd driver so should not be freed. That is the
 * ready for all the const stuff in the function signature
 *
 * @return list of string points, terminated by NULL; or NULL if no boot order
 */
const char *const *const bootstd_get_order(struct udevice *dev);

#endif
