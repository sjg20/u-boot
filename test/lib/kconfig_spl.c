// SPDX-License-Identifier: GPL-2.0+
/*
 * Test of linux/kconfig.h macros for SPL
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <test/lib.h>
#include <test/test.h>
#include <test/ut.h>

static int lib_test_spl_is_enabled(struct unit_test_state *uts)
{
	ut_asserteq(0, CONFIG_IS_ENABLED(CMDLINE))
	ut_asserteq(1, CONFIG_IS_ENABLED(OF_PLATDATA))
	ut_asserteq(0, CONFIG_IS_ENABLED(_UNDEFINED))

	ut_asserteq(0xc000,
		    CONFIG_IF_ENABLED_INT(BLOBLIST_FIXED, BLOBLIST_ADDR));

	return 0;
}
LIB_TEST(lib_test_spl_is_enabled, 0);
