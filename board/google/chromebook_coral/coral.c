// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <acpi.h>
#include <dm.h>

int arch_misc_init(void)
{
	return 0;
}

/* This function is needed if CONFIG_CMDLINE is not enabled */
int board_run_command(const char *cmdline)
{
	printf("No command line\n");

	return 0;
}

static int coral_write_acpi_tables(struct udevice *dev, struct acpi_ctx *ctx)
{
	/* Add NHLT here */

	return 0;
}

struct acpi_ops coral_acpi_ops = {
	.write_tables	= coral_write_acpi_tables,
};

static const struct udevice_id coral_ids[] = {
	{ .compatible = "google,coral" },
	{ }
};

U_BOOT_DRIVER(coral_drv) = {
	.name		= "coral",
	.id		= UCLASS_BOARD,
	.of_match	= coral_ids,
	acpi_ops_ptr(&coral_acpi_ops)
};
