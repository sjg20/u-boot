// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for bootdev functions. All start with 'bootdev'
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <test/suites.h>
#include <test/ut.h>
#include "bootdev_common.h"

/* Check 'bootflow scan/list' commands */
static int bootflow_cmd(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootdev select 1", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan -l", 0));
	ut_assert_nextline("Scanning for bootflows in bootdev 'mmc1.bootdev'");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     ready   mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing bootflows for bootdev 'mmc1.bootdev'");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     ready   mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_cmd, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan' with a name / label / seq */
static int bootflow_cmd_label(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -l mmc1", 0));
	ut_assert_nextline("Scanning for bootflows in bootdev 'mmc1.bootdev'");
	ut_assert_skip_to_line("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow scan -l mmc0.bootdev", 0));
	ut_assert_nextline("Scanning for bootflows in bootdev 'mmc0.bootdev'");
	ut_assert_skip_to_line("(0 bootflows, 0 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow scan -l 0", 0));
	ut_assert_nextline("Scanning for bootflows in bootdev 'mmc2.bootdev'");
	ut_assert_skip_to_line("(0 bootflows, 0 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_cmd_label, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan/list' commands using all bootdevs */
static int bootflow_cmd_glob(struct unit_test_state *uts)
{
	ut_assertok(bootdev_test_drop_boot_order(uts));

	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -l", 0));
	ut_assert_nextline("Scanning for bootflows in all bootdevs");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("Scanning bootdev 'mmc2.bootdev':");
	ut_assert_nextline("Scanning bootdev 'mmc1.bootdev':");
	ut_assert_nextline("  0  syslinux     ready   mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextline("Scanning bootdev 'mmc0.bootdev':");
	ut_assert_nextline("No more bootdevs");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing all bootflows");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     ready   mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_cmd_glob, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan -e' */
static int bootflow_cmd_scan_e(struct unit_test_state *uts)
{
	ut_assertok(bootdev_test_drop_boot_order(uts));

	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -ale", 0));
	ut_assert_nextline("Scanning for bootflows in all bootdevs");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("Scanning bootdev 'mmc2.bootdev':");
	ut_assert_nextline("  0  syslinux     media   mmc          0  mmc2.bootdev.whole        <NULL>");
	ut_assert_nextline("     ** No partition found, err=-93");
	ut_assert_nextline("  1  efi          media   mmc          0  mmc2.bootdev.whole        <NULL>");
	ut_assert_nextline("     ** No partition found, err=-93");

	ut_assert_nextline("Scanning bootdev 'mmc1.bootdev':");
	ut_assert_nextline("  2  syslinux     media   mmc          0  mmc1.bootdev.whole        <NULL>");
	ut_assert_nextline("     ** No partition found, err=-2");
	ut_assert_nextline("  3  efi          media   mmc          0  mmc1.bootdev.whole        <NULL>");
	ut_assert_nextline("     ** No partition found, err=-2");
	ut_assert_nextline("  4  syslinux     ready   mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextline("  5  efi          fs      mmc          1  mmc1.bootdev.part_1       efi/boot/bootsbox.efi");

	ut_assert_skip_to_line("Scanning bootdev 'mmc0.bootdev':");
	ut_assert_skip_to_line(" 3f  efi          media   mmc          0  mmc0.bootdev.whole        <NULL>");
	ut_assert_nextline("     ** No partition found, err=-93");
	ut_assert_nextline("No more bootdevs");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(64 bootflows, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing all bootflows");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     media   mmc          0  mmc2.bootdev.whole        <NULL>");
	ut_assert_nextline("  1  efi          media   mmc          0  mmc2.bootdev.whole        <NULL>");
	ut_assert_skip_to_line("  4  syslinux     ready   mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_skip_to_line(" 3f  efi          media   mmc          0  mmc0.bootdev.whole        <NULL>");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(64 bootflows, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_cmd_scan_e, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow info' */
static int bootflow_cmd_info(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootdev select 1", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow select 0", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow info", 0));
	ut_assert_nextline("Name:      mmc1.bootdev.part_1");
	ut_assert_nextline("Device:    mmc1.bootdev");
	ut_assert_nextline("Block dev: mmc1.blk");
	ut_assert_nextline("Sequence:  0");
	ut_assert_nextline("Method:    syslinux");
	ut_assert_nextline("State:     ready");
	ut_assert_nextline("Partition: 1");
	ut_assert_nextline("Subdir:    (none)");
	ut_assert_nextline("Filename:  extlinux/extlinux.conf");
	ut_assert_nextlinen("Buffer:    ");
	ut_assert_nextline("Size:      253 (595 bytes)");
	ut_assert_nextline("Error:     0");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow info -d", 0));
	ut_assert_nextline("Name:      mmc1.bootdev.part_1");
	ut_assert_skip_to_line("Error:     0");
	ut_assert_nextline("Contents:");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("# extlinux.conf generated by appliance-creator");
	ut_assert_skip_to_line("        initrd /initramfs-5.3.7-301.fc31.armv7hl.img");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_cmd_info, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan -b' to boot the first available bootdev */
static int bootflow_scan_boot(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -b", 0));
	ut_assert_nextline("** Booting bootflow 'mmc1.bootdev.part_1'");
	ut_assert_nextline("Ignoring unknown command: ui");

	/*
	 * We expect it to get through to boot although sandbox always returns
	 * -EFAULT as it cannot actually boot the kernel
	 */
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_nextline("Boot failed (err=-14)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_scan_boot, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow boot' to boot a selected bootflow */
static int bootflow_cmd_boot(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootdev select 1", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow select 0", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow boot", 0));
	ut_assert_nextline("** Booting bootflow 'mmc1.bootdev.part_1'");
	ut_assert_nextline("Ignoring unknown command: ui");

	/*
	 * We expect it to get through to boot although sandbox always returns
	 * -EFAULT as it cannot actually boot the kernel
	 */
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_nextline("Boot failed (err=-14)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootflow_cmd_boot, UT_TESTF_DM | UT_TESTF_SCAN_FDT);
