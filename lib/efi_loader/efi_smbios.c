// SPDX-License-Identifier: GPL-2.0+
/*
 *  EFI application tables support
 *
 *  Copyright (c) 2016 Alexander Graf
 */

#define LOG_CATEGORY LOGC_EFI

#include <common.h>
#include <efi_loader.h>
#include <log.h>
#include <mapmem.h>
#include <smbios.h>

/*
 * Install the SMBIOS table as a configuration table.
 *
 * Return:	status code
 */
efi_status_t efi_smbios_register(void)
{
	ulong addr;

	/* space for all tables is marked in efi_acpi_register() */
	addr = gd->arch.smbios_start;
	printf("EFI using SMBIOS tables at %lx\n", addr);

	/* Install SMBIOS information as configuration table */
	return efi_install_configuration_table(&smbios_guid,
					       map_sysmem(addr, 0));
}
