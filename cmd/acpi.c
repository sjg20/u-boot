// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */
#include <common.h>
#include <command.h>

static int do_acpi_list(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{

	return 0;
}

static char acpi_help_text[] =
	"list - list ACPI tables\n"
	"acpi dump <name> - Dump ACPI table";

U_BOOT_CMD_WITH_SUBCMDS(acpi, "ACPI tables", acpi_help_text,
	U_BOOT_SUBCMD_MKENT(list, 1, 1, do_acpi_list),
	U_BOOT_SUBCMD_MKENT(dump, 2, 1, do_acpi_dump);
