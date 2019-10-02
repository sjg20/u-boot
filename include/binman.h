/* SPDX-License-Identifier: Intel */
/*
 * Access to binman information at runtime
 *
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _BINMAN_H_
#define _BINMAN_H_

/**
 *struct binman_entry - information about a binman entry
 *
 * @image_pos: Position of entry in the image
 * @size: Size of entry
 */
struct binman_entry {
	u32 image_pos;
	u32 size;
};

int binman_entry_find(const char *name, struct binman_entry *entry);

int binman_init(void);

#endif
