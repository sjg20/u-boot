// SPDX-License-Identifier: GPL-2.0+
/*
 * UART support for U-Boot when launched from Coreboot
 *
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <ns16550.h>
#include <serial.h>
#include <asm/cb_sysinfo.h>

static const struct pci_device_id ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_APL_UART2) },
	{},
};

/*
 * Coreboot only sets up the UART if it uses it and doesn't bother to put the
 * details in sysinfo if it doesn't. Try to guess in that case, using devices
 * we know about
 *
 * @plat: Platform data to fill in
 * @return 0 if found, -ve if no UART was found
 */
static int guess_uart(struct ns16550_plat *plat)
{
	struct udevice *bus, *dev;
	ulong addr;
	int index;
	int ret;

	ret = uclass_first_device_err(UCLASS_PCI, &bus);
	if (ret)
		return ret;
	index = 0;
	ret = pci_bus_find_devices(bus, ids, &index, &dev);
	if (ret)
		return ret;
	addr = dm_pci_read_bar32(dev, 0);
	plat->base = addr;
	plat->reg_shift = 2;
	plat->reg_width = 4;
	plat->clock = 1843200;
	plat->fcr = UART_FCR_DEFVAL;
	plat->flags = 0;

	return 0;
}

static int coreboot_of_to_plat(struct udevice *dev)
{
	struct ns16550_plat *plat = dev_get_plat(dev);
	struct cb_serial *cb_info = lib_sysinfo.serial;

	if (cb_info) {
		plat->base = cb_info->baseaddr;
		plat->reg_shift = cb_info->regwidth == 4 ? 2 : 0;
		plat->reg_width = cb_info->regwidth;
		plat->clock = cb_info->input_hertz;
		plat->fcr = UART_FCR_DEFVAL;
		plat->flags = 0;
		if (cb_info->type == CB_SERIAL_TYPE_IO_MAPPED)
			plat->flags |= NS16550_FLAG_IO;
	} else {
		int ret;

		ret = guess_uart(plat);
		if (ret) {
			/*
			 * Returning an error will cause U-Boot to complain that
			 * there is no UART, which may panic. So stay silent and
			 * pray that the video console will work.
			 */
			log_debug("Cannot detect UART\n");
		}
	}

	return 0;
}

static const struct udevice_id coreboot_serial_ids[] = {
	{ .compatible = "coreboot-serial" },
	{ },
};

U_BOOT_DRIVER(coreboot_uart) = {
	.name	= "coreboot_uart",
	.id	= UCLASS_SERIAL,
	.of_match	= coreboot_serial_ids,
	.priv_auto	= sizeof(struct ns16550),
	.plat_auto	= sizeof(struct ns16550_plat),
	.of_to_plat  = coreboot_of_to_plat,
	.probe	= ns16550_serial_probe,
	.ops	= &ns16550_serial_ops,
	.flags	= DM_FLAG_PRE_RELOC,
};
