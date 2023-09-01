// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
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

	memset(&lst, '\xff', sizeof(lst));
	ut_assert(alist_init(&lst, 0));
	ut_asserteq_ptr(NULL, lst.ptrs);
#if 0
	abuf_set(&buf, test_data, TEST_DATA_LEN);
	ut_asserteq_ptr(test_data, buf.data);
	ut_asserteq(TEST_DATA_LEN, buf.size);
	ut_asserteq(false, buf.alloced);

	/* Force it to allocate */
	ut_asserteq(true, abuf_realloc(&buf, TEST_DATA_LEN + 1));
	ut_assertnonnull(buf.data);
	ut_asserteq(TEST_DATA_LEN + 1, buf.size);
	ut_asserteq(true, buf.alloced);

	/* Now set it again, to force it to free */
	abuf_set(&buf, test_data, TEST_DATA_LEN);
	ut_asserteq_ptr(test_data, buf.data);
	ut_asserteq(TEST_DATA_LEN, buf.size);
	ut_asserteq(false, buf.alloced);
#endif
	/* Check for memory leaks */
	ut_assertok(ut_check_delta(start));

	return 0;
}
LIB_TEST(lib_test_alist_init, 0);
