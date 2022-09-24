// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for `vbe` command
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <vbe.h>
#include <test/suites.h>
#include <test/ut.h>
#include "bootstd_common.h"

/* Check 'vbe list' command */
static int vbe_cmd_list(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("vbe list", 0));
	ut_assert_nextline("  #  Sel  Device           Driver          Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  2       firmware0        vbe_simple      VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(vbe_cmd_list, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'vbe select' command */
static int vbe_cmd_select(struct unit_test_state *uts)
{
	/* select a device */
	console_record_reset_enable();
	ut_assertok(run_command("vbe select 2", 0));
	ut_assert_console_end();

	ut_assertok(run_command("vbe list", 0));
	ut_assert_nextline("  #  Sel  Device           Driver          Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  2  *    firmware0        vbe_simple      VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_console_end();

	/* deselect it */
	ut_assertok(run_command("vbe select", 0));
	ut_assert_console_end();
	ut_assertok(run_command("vbe list", 0));
	ut_assert_nextline("  #  Sel  Device           Driver          Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  2       firmware0        vbe_simple      VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_console_end();

	/* select a device by name */
	console_record_reset_enable();
	ut_assertok(run_command("vbe select firmware0", 0));
	ut_assert_console_end();
	ut_assertok(run_command("vbe list", 0));
	ut_assert_nextline("  #  Sel  Device           Driver          Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  2  *    firmware0        vbe_simple      VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(vbe_cmd_select, UT_TESTF_DM | UT_TESTF_SCAN_FDT);
