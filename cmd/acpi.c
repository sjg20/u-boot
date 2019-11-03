// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */
#include <common.h>
#include <acpi.h>
#include <command.h>
#include <asm/acpi_table.h>

static void dump_hdr(struct acpi_table_header *hdr)
{
	bool has_hdr = memcmp(hdr->signature, "FACS", ACPI_SIG_LEN);

	printf("%.*s %08lx %06x", ACPI_SIG_LEN, hdr->signature, (ulong)hdr,
	       hdr->length);
	if (has_hdr) {
		printf(" (v%02d %.6s %.8s %u %.4s %d)\n", hdr->revision,
		       hdr->oem_id, hdr->oem_table_id, hdr->oem_revision,
		       hdr->aslc_id, hdr->aslc_revision);
	} else {
		printf("\n");
	}
}

/**
 * find_table() - Look up an ACPI table
 *
 * @sig: Signature of table (4 characters, upper case)
 * @return pointer to table header, or NULL if not found
 */
struct acpi_table_header *find_table(const char *sig)
{
	struct acpi_rsdp *rsdp;
	struct acpi_rsdt *rsdt;
	int len, i, count;

	rsdp = (struct acpi_rsdp *)gd->arch.acpi_start;
	if (!rsdp)
		return NULL;
	rsdt = (struct acpi_rsdt *)rsdp->rsdt_address;
	len = rsdt->header.length - sizeof(rsdt->header);
	count = len / sizeof(u32);
	for (i = 0; i < count; i++) {
		struct acpi_table_header *hdr;

		hdr = (struct acpi_table_header *)rsdt->entry[i];
		if (!memcmp(hdr->signature, sig, ACPI_SIG_LEN))
			return hdr;
		if (!memcmp(hdr->signature, "FACP", ACPI_SIG_LEN)) {
			struct acpi_fadt *fadt = (struct acpi_fadt *)hdr;

			if (!memcmp(sig, "DSDT", ACPI_SIG_LEN) && fadt->dsdt)
				return (struct acpi_table_header *)fadt->dsdt;
			if (!memcmp(sig, "FACS", ACPI_SIG_LEN) &&
			    fadt->firmware_ctrl)
				return (struct acpi_table_header *)fadt->
					firmware_ctrl;
		}
	}

	return NULL;
}

static int dump_table_name(const char *sig)
{
	struct acpi_table_header *hdr;

	hdr = find_table(sig);
	if (!hdr)
		return -ENOENT;
	printf("%.*s @ %p\n", ACPI_SIG_LEN, hdr->signature, hdr);
	print_buffer(0, hdr, 1, hdr->length, 0);

	return 0;
}

static void list_fact(struct acpi_fadt *fadt)
{
	if (fadt->dsdt)
		dump_hdr((struct acpi_table_header *)fadt->dsdt);
	if (fadt->firmware_ctrl)
		dump_hdr((struct acpi_table_header *)fadt->firmware_ctrl);
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
		if (!memcmp(hdr->signature, "FACP", ACPI_SIG_LEN))
			list_fact((struct acpi_fadt *)hdr);
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
	if (!rsdp) {
		printf("No ACPI tables present\n");
		return 0;
	}
	printf("ACPI tables start at %p\n", rsdp);
	list_rsdp(rsdp);

	return 0;
}

static int do_acpi_split(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	acpi_dump_items();

	return 0;
}

static int do_acpi_dump(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	const char *name;
	char sig[ACPI_SIG_LEN];
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;
	name = argv[1];
	if (strlen(name) != ACPI_SIG_LEN) {
		printf("Table name '%s' must be four characters\n", name);
		return CMD_RET_FAILURE;
	}
	str_to_upper(name, sig);
	ret = dump_table_name(sig);
	if (ret) {
		printf("Table '%.*s' not found\n", ACPI_SIG_LEN, sig);
		return CMD_RET_FAILURE;
	}

	return 0;
}

static char acpi_help_text[] =
	"list - list ACPI tables\n"
	"acpi dump <name> - Dump ACPI table";

U_BOOT_CMD_WITH_SUBCMDS(acpi, "ACPI tables", acpi_help_text,
	U_BOOT_SUBCMD_MKENT(list, 1, 1, do_acpi_list),
	U_BOOT_SUBCMD_MKENT(split, 1, 1, do_acpi_split),
	U_BOOT_SUBCMD_MKENT(dump, 2, 1, do_acpi_dump));
