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

	/* Clear it to avoid any confusion */
	memset(lst, '\0', sizeof(struct alist));
}
