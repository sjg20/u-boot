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

#define PTR0	(void *)10
#define PTR1	(void *)1
#define PTR2	(void *)2
#define PTR3	(void *)3

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

/* Test alist_add() */
static int lib_test_alist_add(struct unit_test_state *uts)
{
	struct alist lst;
	ulong start;

	start = ut_check_free();
	ut_assert(alist_init(&lst, 0));
	ut_assert(alist_add(&lst, PTR0));
	ut_assert(alist_add(&lst, PTR1));
	ut_assert(alist_add(&lst, PTR2));
	ut_assert(alist_add(&lst, PTR3));
	ut_assertnonnull(lst.ptrs);
	ut_asserteq(4, lst.size);
	ut_asserteq(4, lst.alloc);

	ut_asserteq_ptr(PTR0, lst.ptrs[0]);
	ut_asserteq_ptr(PTR1, lst.ptrs[1]);
	ut_asserteq_ptr(PTR2, lst.ptrs[2]);
	ut_asserteq_ptr(PTR3, lst.ptrs[3]);

	/* add another and check that things look right */
	ut_assert(alist_add(&lst, PTR0));
	ut_asserteq(5, lst.size);
	ut_asserteq(8, lst.alloc);

	ut_asserteq_ptr(PTR0, lst.ptrs[0]);
	ut_asserteq_ptr(PTR1, lst.ptrs[1]);
	ut_asserteq_ptr(PTR2, lst.ptrs[2]);
	ut_asserteq_ptr(PTR3, lst.ptrs[3]);

	ut_asserteq_ptr(PTR0, lst.ptrs[4]);
	ut_assertnull(lst.ptrs[5]);
	ut_assertnull(lst.ptrs[6]);
	ut_assertnull(lst.ptrs[7]);

	/* add some more, checking handling of malloc() failure */
	malloc_enable_testing(0);
	ut_assert(alist_add(&lst, PTR1));
	ut_assert(alist_add(&lst, PTR2));
	ut_assert(alist_add(&lst, PTR3));
	ut_asserteq(false, alist_add(&lst, PTR0));
	malloc_disable_testing();

	/* make sure nothing changed */
	ut_asserteq(8, lst.size);
	ut_asserteq(8, lst.alloc);
	ut_asserteq_ptr(PTR0, lst.ptrs[0]);
	ut_asserteq_ptr(PTR1, lst.ptrs[1]);
	ut_asserteq_ptr(PTR2, lst.ptrs[2]);
	ut_asserteq_ptr(PTR3, lst.ptrs[3]);
	ut_asserteq_ptr(PTR0, lst.ptrs[4]);
	ut_asserteq_ptr(PTR1, lst.ptrs[5]);
	ut_asserteq_ptr(PTR2, lst.ptrs[6]);
	ut_asserteq_ptr(PTR3, lst.ptrs[7]);

	alist_uninit(&lst);

	/* Check for memory leaks */
	ut_assertok(ut_check_delta(start));

	return 0;
}
LIB_TEST(lib_test_alist_add, 0);

/* Test alist_set() */
static int lib_test_alist_set(struct unit_test_state *uts)
{
	struct alist lst;
	ulong start;

	start = ut_check_free();

	ut_assert(alist_init(&lst, 0));
	ut_assert(alist_set(&lst, 2, PTR2));
	ut_asserteq(3, lst.size);
	ut_asserteq(4, lst.alloc);
	ut_assertnull(lst.ptrs[0]);
	ut_assertnull(lst.ptrs[1]);
	ut_asserteq_ptr(PTR2, lst.ptrs[2]);
	ut_assertnull(lst.ptrs[3]);
	alist_uninit(&lst);

	/* Check for memory leaks */
	ut_assertok(ut_check_delta(start));

	return 0;
}
LIB_TEST(lib_test_alist_set, 0);
