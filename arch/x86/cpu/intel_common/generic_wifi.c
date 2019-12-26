/*
 * This file is part of the coreboot project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 or (at your option)
 * any later version of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <dm.h>
#include <asm/acpigen.h>
#include <asm/generic_wifi.h>

#if 0
#include <arch/acpi_device.h>
#include <arch/acpigen.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci_def.h>
#include <sar.h>
#include <string.h>
#include <wrdd.h>
#include "generic_wifi.h"
#endif

/* WRDS Spec Revision */
#define WRDS_REVISION 0x0

/* EWRD Spec Revision */
#define EWRD_REVISION 0x0

/* WRDS Domain type */
#define WRDS_DOMAIN_TYPE_WIFI 0x7

/* EWRD Domain type */
#define EWRD_DOMAIN_TYPE_WIFI 0x7

/* WGDS Domain type */
#define WGDS_DOMAIN_TYPE_WIFI 0x7

/*
 * WIFI ACPI NAME = "WF" + hex value of last 8 bits of dev_path_encode + '\0'
 * The above representation returns unique and consistent name every time
 * generate_wifi_acpi_name is invoked. The last 8 bits of dev_path_encode is
 * chosen since it contains the bus address of the device.
 */
#define WIFI_ACPI_NAME_MAX_LEN 5

int generic_wifi_fill_ssdt(struct udevice *dev,
			   const struct generic_wifi_config *config)
{
	const char *path;
	char name[ACPI_DEVICE_NAME_MAX];
	pci_dev_t bdf;
	u32 address;
	int ret;

	path = acpi_device_path(dev_get_parent(dev));
	if (!path)
		return -ENXIO;
	ret = acpi_device_name(dev, name);
	if (ret)
		return log_msg_ret("name", ret);

	/* Device */
	acpigen_write_scope(path);
	acpigen_write_device(name);
	acpigen_write_name_integer("_UID", 0);
	acpigen_write_name_string("_DDN", dev_read_string(dev, "acpi-ddn"));

	/* Address */
	bdf = dm_pci_get_bdf(dev);
	address = (PCI_DEV(bdf) << 16) | PCI_FUNC(bdf);
	acpigen_write_name_dword("_ADR", address);

	/* Wake capabilities */
	if (config)
		acpigen_write_prw(config->wake, config->maxsleep);

	acpigen_pop_len(); /* Device */
	acpigen_pop_len(); /* Scope */

	return 0;
}
