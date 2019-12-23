// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */
#include <common.h>
#include <command.h>
#include <asm/acpi_table.h>

static int dump_rsdp(struct acpi_rsdp *rsdp)
{
	printf("RSDP:\n");
	printf("%-20s%.8s\n", "Signature:", rsdp->signature);

	return 0;
}

static void dump_hdr(struct acpi_table_header *hdr)
{
	printf("%.4s %08lx %06x (v%02d %.6s %.8s %u %.4s %d)\n", hdr->signature,
	       (ulong)hdr, hdr->length, hdr->revision, hdr->oem_id,
	       hdr->oem_table_id, hdr->oem_revision, hdr->aslc_id,
	       hdr->aslc_revision);
}

static int list_rsdt(struct acpi_rsdt *rsdt, struct acpi_xsdt *xsdt)
{
	int len, i, count;

	dump_hdr(&rsdt->header);
	if (xsdt)
		dump_hdr(&xsdt->header);
	len = rsdt->header.length - sizeof(rsdt->header);
	count = len / sizeof(u32);
	for (i = 0; i < count; i++) {
		struct acpi_table_header *hdr;

		hdr = (struct acpi_table_header *)rsdt->entry[i];
		dump_hdr(hdr);
		if (xsdt) {
			if (xsdt->entry[i] != rsdt->entry[i]) {
				printf("   (xsdt mismatch %llx)\n",
				       xsdt->entry[i]);
			}
		}
	}

	return 0;
}

static int list_rsdp(struct acpi_rsdp *rsdp)
{
	struct acpi_rsdt *rsdt;
	struct acpi_xsdt *xsdt;

	printf("RSDP %08lx %06x (v%02d %.6s)\n", (ulong)rsdp, rsdp->length,
	       rsdp->revision, rsdp->oem_id);
	rsdt = (struct acpi_rsdt *)rsdp->rsdt_address;
	xsdt = (struct acpi_xsdt *)(ulong)rsdp->xsdt_address;
	list_rsdt(rsdt, xsdt);

	return 0;
}

static int do_acpi_list(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct acpi_rsdp *rsdp;

	rsdp = (struct acpi_rsdp *)gd->arch.acpi_start;
	printf("ACPI tables start at %p\n", rsdp);
	list_rsdp(rsdp);

	return 0;
}

static int do_acpi_dump(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	return 0;
}

static char acpi_help_text[] =
	"list - list ACPI tables\n"
	"acpi dump <name> - Dump ACPI table";

U_BOOT_CMD_WITH_SUBCMDS(acpi, "ACPI tables", acpi_help_text,
	U_BOOT_SUBCMD_MKENT(list, 1, 1, do_acpi_list),
	U_BOOT_SUBCMD_MKENT(dump, 2, 1, do_acpi_dump));
