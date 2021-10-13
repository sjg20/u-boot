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
	char *const *prefixes;
	char *const *order;
};

char *const *bootstd_get_order(struct udevice *dev);

#endif
