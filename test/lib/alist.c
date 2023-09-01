// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <alist.h>
#include <string.h>
#include <test/lib.h>
#include <test/test.h>
#include <test/ut.h>

/* Test alist_init() */
static int lib_test_alist_init(struct unit_test_state *uts)
{
	struct alist lst;
	ulong start;

	start = ut_check_free();

	/* with a size of 0, the fields should be inited, with no memory used */
	memset(&lst, '\xff', sizeof(lst));
	ut_assert(alist_init(&lst, 0));
	ut_asserteq_ptr(NULL, lst.ptrs);
	ut_asserteq(0, lst.size);
	ut_asserteq(0, lst.alloc);
	ut_assertok(ut_check_delta(start));
	alist_uninit(&lst);
	ut_asserteq_ptr(NULL, lst.ptrs);
	ut_asserteq(0, lst.size);
	ut_asserteq(0, lst.alloc);

	/* use an impossible size */
	ut_asserteq(false, alist_init(&lst, CONFIG_SYS_MALLOC_LEN));
	ut_assertnull(lst.ptrs);
	ut_asserteq(0, lst.size);
	ut_asserteq(0, lst.alloc);

	/* use a small size */
	ut_assert(alist_init(&lst, 4));
	ut_assertnonnull(lst.ptrs);
	ut_asserteq(0, lst.size);
	ut_asserteq(4, lst.alloc);

	/* free it */
	alist_uninit(&lst);
	ut_asserteq_ptr(NULL, lst.ptrs);
	ut_asserteq(0, lst.size);
	ut_asserteq(0, lst.alloc);
	ut_assertok(ut_check_delta(start));

	/* Check for memory leaks */
	ut_assertok(ut_check_delta(start));

	return 0;
}
LIB_TEST(lib_test_alist_init, 0);
