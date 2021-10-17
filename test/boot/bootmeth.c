// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for bootdev functions. All start with 'bootdev'
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <test/suites.h>
#include <test/ut.h>
#include "bootdev_common.h"

/* Check 'bootmeth list' command */
static int bootmeth_cmd_list(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux            Syslinux boot from a block device");
	ut_assert_nextline("  1  efi                 EFI boot from a .efi file");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootmeth_cmd_list, UT_TESTF_DM | UT_TESTF_SCAN_FDT);
