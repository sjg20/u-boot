// SPDX-License-Identifier: GPL-2.0+
/*
 * Handles a contiguous list of pointers which be allocated and freed
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <alist.h>
#include <malloc.h>
#include <string.h>

enum {
	ALIST_INITIAL_SIZE	= 4,	/* default size of unsized list */
};

bool alist_init(struct alist *lst, uint start_size)
{
	/* Avoid realloc for the initial size to help malloc_simple */
	if (start_size) {
		lst->ptrs = calloc(sizeof(void *), start_size);
		if (!lst->ptrs)
			return false;
		lst->alloc = start_size;
		lst->size = 0;
	} else {
		memset(lst, '\0', sizeof(struct alist));
	}

	return true;
}

void alist_uninit(struct alist *lst)
{
	free(lst->ptrs);

	/* Clear fields to avoid any confusion */
	memset(lst, '\0', sizeof(struct alist));
}

/**
 * alist_expand() - Expand a list to at least the given size
 *
 * @lst: List to modify
 * @inc_by: Amount to expand by
 * Return: true if OK, false if out of memory
 */
bool alist_expand(struct alist *lst, uint inc_by)
{
	uint new_alloc = lst->alloc + inc_by;
	void *new_ptrs;

	new_ptrs = realloc(lst->ptrs, sizeof(void *) * new_alloc);
	if (!new_ptrs)
		return false;
	memset(new_ptrs + sizeof(void *) * lst->alloc, '\0',
	       sizeof(void *) * inc_by);
	lst->alloc = new_alloc;
	lst->ptrs = new_ptrs;

	return true;
}

bool alist_add(struct alist *lst, void *ptr)
{
	if (lst->size == lst->alloc &&
	    !alist_expand(lst, lst->alloc ?: ALIST_INITIAL_SIZE))
		return false;

	lst->ptrs[lst->size++] = ptr;

	return true;
}

bool alist_set(struct alist *lst, uint index, void *ptr)
{
	uint minsize = index + 1;

	if (minsize > lst->alloc && !alist_expand(lst, minsize))
		return false;

	lst->ptrs[index] = ptr;
	if (minsize >= lst->size)
		lst->size = minsize;

	return true;
}
