/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Handles a contiguous list of pointers which be allocated and freed
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __ALIST_H
#define __ALIST_H

#include <stdbool.h>
#include <linux/types.h>

/**
 * struct alist - point list that can be allocated and freed
 *
 * This is useful for a list of pointers which may need to change in size.
 *
 * @ptrs: Array of pointers
 * @len: Length of array
 * @alloc: allocated length of array, to which @len can grow
 */
struct alist {
	void **ptrs;
	uint size;
	uint alloc;
};

/**
 * alist_init() - Set up a new pointer list
 *
 * @lst: List to set up
 * @alloc_size: Number of items to allow to start
 * Return: true if OK, false if out of memory
 */
bool alist_init(struct alist *lst, uint alloc_size);

/**
 * alist_uninit() - Free any memory used by an alist
 *
 * The alist must be inited before this can be called.
 *
 * @alist: alist to uninit
 */
void alist_uninit(struct alist *alist);

#endif /* __ALIST_H */
