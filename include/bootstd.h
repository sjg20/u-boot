/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootstd_h
#define __bootstd_h

/**
 * struct bootstd_plat - plat data for the bootstd driver
 *
 * @prefixes: NULL-terminated list of prefixes to use for bootflow filenames,
 *	e.g. "/", "/boot/"
 * @order: Order to use for bootdevs, e.g. "mmc2", "mmc1"
 */
struct bootstd_plat {
	char **prefixes;
	char **order;
};

#endif
