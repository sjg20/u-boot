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
#include "bootstd_common.h"

/* Check 'bootmeth list' command */
static int bootmeth_cmd_list(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    0  syslinux            Syslinux boot from a block device");
	ut_assert_nextline("    1    1  efi                 EFI boot from a .efi file");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_list, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootmeth order' command */
static int bootmeth_cmd_order(struct unit_test_state *uts)
{
	/* Select just one bootmethod */
	console_record_reset_enable();
	ut_assertok(run_command("bootmeth order syslinux", 0));
	ut_assert_console_end();

	/* Only that one should be listed */
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    0  syslinux            Syslinux boot from a block device");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootmeth)");
	ut_assert_console_end();

	/* Check the -a flag, efi should show as not in the order ("-") */
	ut_assertok(run_command("bootmeth list -a", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    0  syslinux            Syslinux boot from a block device");
	ut_assert_nextline("    -    1  efi                 EFI boot from a .efi file");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assert_console_end();

	/* Check the -a flag with the reverse order */
	ut_assertok(run_command("bootmeth order efi syslinux", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootmeth list -a", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    1    0  syslinux            Syslinux boot from a block device");
	ut_assert_nextline("    0    1  efi                 EFI boot from a .efi file");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assert_console_end();

	/* Now reset the order to empty, which should show all of them again */
	ut_assertok(run_command("bootmeth order", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_skip_to_line("(2 bootmeths)");

	/* Try reverse order */
	ut_assertok(run_command("bootmeth order efi syslinux", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    1  efi                 EFI boot from a .efi file");
	ut_assert_nextline("    1    0  syslinux            Syslinux boot from a block device");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_order, UT_TESTF_DM | UT_TESTF_SCAN_FDT);
