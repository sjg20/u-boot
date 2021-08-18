// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <bootdev.h>
#include <bootflow.h>
#include <mapmem.h>
#include <os.h>
#include <test/suites.h>
#include <test/ut.h>

/* Declare a new bootdev test */
#define BOOTDEV_TEST(_name, _flags) \
		UNIT_TEST(_name, _flags, bootdev_test)

/* Check 'bootdev list' command */
static int bootdev_test_cmd_list(struct unit_test_state *uts)
{
	int probed;

	console_record_reset_enable();
	for (probed = 0; probed < 2; probed++) {
		int probe_ch = probed ? '+' : ' ';

		ut_assertok(run_command(probed ? "bootdev list -p" :
			"bootdev list", 0));
		ut_assert_nextline("Seq  Probed  Status  Uclass    Name");
		ut_assert_nextlinen("---");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 0, probe_ch, "OK",
				   "mmc", "mmc2.bootdev");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 1, probe_ch, "OK",
				   "mmc", "mmc1.bootdev");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 2, probe_ch, "OK",
				   "mmc", "mmc0.bootdev");
		ut_assert_nextlinen("---");
		ut_assert_nextline("(3 devices)");
		ut_assert_console_end();
	}

	return 0;
}
BOOTDEV_TEST(bootdev_test_cmd_list, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootdev select' and 'info' commands */
static int bootdev_test_cmd_select(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_asserteq(1, run_command("bootdev info", 0));
	ut_assert_nextlinen("Please use");
	ut_assert_console_end();

	ut_assertok(run_command("bootdev select 0", 0));
	ut_assert_console_end();

	ut_assertok(run_command("bootdev info", 0));
	ut_assert_nextline("Name:      mmc2.bootdev");
	ut_assert_nextline("Sequence:  0");
	ut_assert_nextline("Status:    Probed");
	ut_assert_nextline("Uclass:    mmc");
	ut_assert_nextline("Bootflows: 0 (0 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootdev_test_cmd_select, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan/list' commands */
static int bootdev_test_cmd_bootflow(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootdev select 1", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan -l", 0));
	ut_assert_nextline("Scanning for bootflows in bootdev 'mmc1.bootdev'");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     loaded  mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing bootflows for bootdev 'mmc1.bootdev'");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     loaded  mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootdev_test_cmd_bootflow, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan/list' commands using all bootdevs */
static int bootdev_test_cmd_bootflow_glob(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -l", 0));
	ut_assert_nextline("Scanning for bootflows in all bootdevs");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("Scanning bootdev 'mmc2.bootdev':");
	ut_assert_nextline("Scanning bootdev 'mmc1.bootdev':");
	ut_assert_nextline("  0  syslinux     loaded  mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextline("Scanning bootdev 'mmc0.bootdev':");
	ut_assert_nextline("No more bootdevs");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing all bootflows");
	ut_assert_nextline("Seq  Method       State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  syslinux     loaded  mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootdev_test_cmd_bootflow_glob, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan -e' */
static int bootdev_test_cmd_bootflow_scan_e(struct unit_test_state *uts)
{
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
	ut_assert_nextline("  4  syslinux     loaded  mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
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
	ut_assert_skip_to_line("  4  syslinux     loaded  mmc          1  mmc1.bootdev.part_1       extlinux/extlinux.conf");
	ut_assert_skip_to_line(" 3f  efi          media   mmc          0  mmc0.bootdev.whole        <NULL>");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(64 bootflows, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTDEV_TEST(bootdev_test_cmd_bootflow_scan_e, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow info' */
static int bootdev_test_cmd_bootflow_info(struct unit_test_state *uts)
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
	ut_assert_nextline("State:     loaded");
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
BOOTDEV_TEST(bootdev_test_cmd_bootflow_info, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan -b' to boot the first available bootdev */
static int bootdev_test_cmd_bootflow_scan_boot(struct unit_test_state *uts)
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
BOOTDEV_TEST(bootdev_test_cmd_bootflow_scan_boot,
	     UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow boot' to boot a selected bootflow */
static int bootdev_test_cmd_bootflow_boot(struct unit_test_state *uts)
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
BOOTDEV_TEST(bootdev_test_cmd_bootflow_boot, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check we can get a bootdev */
static int bootdev_test_get(struct unit_test_state *uts)
{
	struct bootflow_iter iter;
	struct bootflow bflow;

	ut_assertok(bootflow_scan_first(&iter, 0, &bflow));

	return 0;
}
BOOTDEV_TEST(bootdev_test_get, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

int do_ut_bootdev(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(bootdev_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(bootdev_test);

	return cmd_ut_category("bootdev", "bootdev_test_",
			       tests, n_ents, argc, argv);
}
