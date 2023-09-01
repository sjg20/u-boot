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

bool alist_init(struct alist *lst, uint struct_size, uint start_size)
{
	/* Avoid realloc for the initial size to help malloc_simple */
	if (start_size) {
		lst->ptrs = calloc(sizeof(void *), start_size);
		if (!lst->ptrs)
			return false;
		lst->struct_size = struct_size;
		lst->alloc = start_size;
		lst->count = 0;
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
 * alist_expand_to() - Expand a list to the given size
 *
 * @lst: List to modify
 * @inc_by: Amount to expand to
 * Return: true if OK, false if out of memory
 */
static bool alist_expand_to(struct alist *lst, uint new_alloc)
{
	void *new_ptrs;

	new_ptrs = realloc(lst->ptrs, sizeof(void *) * new_alloc);
	if (!new_ptrs)
		return false;
	memset(new_ptrs + sizeof(void *) * lst->alloc, '\0',
	       sizeof(void *) * (new_alloc - lst->alloc));
	lst->alloc = new_alloc;
	lst->ptrs = new_ptrs;

	return true;
}

/**
 * alist_expand_by() - Expand a list by the given amount
 *
 * @lst: alist to expand
 * @inc_by: Amount to expand by
 * Return: true if OK, false if out of memory
 */
bool alist_expand_by(struct alist *lst, uint inc_by)
{
	return alist_expand_to(lst, lst->alloc + inc_by);
}

/**
 * alist_expand_min() - Expand to at least the provided size
 *
 * Expands to the lowest power of two which can incorporate the new size
 *
 * @lst: alist to expand
 * @min_alloc: Minimum new allocated size; if 0 then ALIST_INITIAL_SIZE is used
 * Return: true if OK, false if out of memory
 */
static bool alist_expand_min(struct alist *lst, uint min_alloc)
{
	uint new_alloc;

	for (new_alloc = lst->alloc ?: ALIST_INITIAL_SIZE;
	     new_alloc < min_alloc;)
		new_alloc *= 2;

	return alist_expand_to(lst, new_alloc);
}

bool alist_addraw(struct alist *lst, void *ptr)
{
	if (lst->count == lst->alloc && !alist_expand_min(lst, lst->count + 1))
		return false;

	lst->ptrs[lst->count++] = ptr;

	return true;
}

bool alist_setraw(struct alist *lst, uint index, void *ptr)
{
	uint minsize = index + 1;

	if (minsize > lst->alloc && !alist_expand_min(lst, minsize))
		return false;

	lst->ptrs[index] = ptr;
	if (minsize >= lst->count)
		lst->count = minsize;

	return true;
}

bool alist_valid(struct alist *lst, uint index)
{
	return index < lst->count;
}

const void *alist_get(struct alist *lst, uint index)
{
	if (index >= lst->count)
		return NULL;

	return lst->ptrs[index];
}

void *alist_addr(struct alist *lst, uint index)
{
	void *ptr;

	if (index > lst->alloc && !alist_expand_min(lst, index + 1))
		return NULL;

	ptr = lst->ptrs[index];
	if (!ptr) {
		ptr = calloc(1, lst->struct_size);
		if (!ptr)
			return NULL;
		lst->ptrs[index] = ptr;
	}

	return ptr;
}
