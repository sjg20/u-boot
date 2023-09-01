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
 * @ptrs: Array of pointers or NULL if not allocated. Array values default to
 * NULL if not assigned
 * @len: Length of array
 * @alloc: allocated length of array, to which @len can grow
 */
struct alist {
	void **ptrs;
	u16 count;
	u16 alloc;
};

/**
 * alist_add() - Add a new pointer the list
 *
 * @lst: alist to update
 * @ptr: Pointer to add
 * Return: true if OK, false if out of memory
 */
bool alist_add(struct alist *lst, void *ptr);

/**
 * alist_set() - Set the value of a pointer
 *
 * @lst: alist to change
 * @index: Index to udpate
 * @ptr: New value to place at position @index
 */
bool alist_set(struct alist *lst, uint index, void *ptr);

/**
 * alist_valid() - Check if an index is within the list range
 *
 * Checks if index is within the current alist count
 *
 * @lst: alist to check
 * @index: Index to check
 * Returns: true if value, else false
 */
bool alist_valid(struct alist *lst, uint index);

/**
 * alist_get() - Get the value of a pointer
 *
 * @lst: alist to check
 * @index: Index to read from
 * Returns: pointer, if present, else NULL
 */
void *alist_get(struct alist *lst, uint index);

/**
 * alist_getd() - Get the value of a pointer directly, with no checking
 *
 * This must only be called on index for which alist_valid() returns true
 *
 * @lst: alist to check
 * @index: Index to read from
 * Returns: pointer value (may be NULL)
 */
static inline void *alist_getd(struct alist *lst, uint index)
{
	return lst->ptrs[index];
}

/**
 * alist_init() - Set up a new pointer list
 *
 * @lst: alist to set up
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
