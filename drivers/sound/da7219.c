/*
 * This file is part of the coreboot project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <acpi.h>
#include <dm.h>
#include <i2c.h>
#include <irq.h>
#include <asm/acpigen.h>
#include <asm/intel_pinctrl.h>
#include <asm-generic/gpio.h>
#include <dm/device-internal.h>
#if 0
#include <arch/acpi.h>
#include <arch/acpi_device.h>
#include <arch/acpigen.h>
#include <console/console.h>
#include <device/i2c_simple.h>
#include <device/device.h>
#include <device/path.h>
#include <stdint.h>

#include "chip.h"
#endif

#define DA7219_ACPI_HID		"DLGS7219"

static int da7219_acpi_fill_ssdt(struct udevice *dev, struct acpi_ctx *ctx)
{
	char scope[ACPI_DEVICE_PATH_MAX];
	char name[ACPI_DEVICE_NAME_MAX];
	struct acpi_dp *dsd, *aad;
	struct acpi_i2c i2c;
	struct irq req_irq;
	u32 val;
	int ret;

	printf("\n\nda7219: before %p\n", acpigen_get_current());

	ret = acpi_device_scope(dev, scope, sizeof(scope));
	if (ret)
		return log_msg_ret("scope", ret);
	ret = acpi_device_name(dev, name);
	if (ret)
		return log_msg_ret("name", ret);

	ret = device_probe(dev);
	if (ret)
		return log_msg_ret("probe", ret);

	/* Device */
	acpigen_write_scope(scope);
	acpigen_write_device(name);
	acpigen_write_name_string("_HID", DA7219_ACPI_HID);
	acpigen_write_name_integer("_UID", 1);
	acpigen_write_name_string("_DDN", dev_read_string(dev, "acpi-ddn"));
	acpigen_write_name_integer("_S0W", 4);
	acpigen_write_sta(acpi_device_status(dev));

	ret = acpi_device_set_i2c(dev, &i2c, scope);
	if (ret)
		return log_msg_ret("i2c", ret);

	/* Resources */
	acpigen_write_name("_CRS");
	acpigen_write_resourcetemplate_header();
	acpi_device_write_i2c(&i2c);

	ret = irq_get_by_index(dev, 0, &req_irq);

	/* Use either Interrupt() or GpioInt() */
	ret = acpi_device_write_interrupt_or_gpio(dev, "req-gpios");
	if (ret)
		return log_msg_ret("irq_gpio", ret);
	acpigen_write_resourcetemplate_footer();

	/* AAD Child Device Properties */
	aad = acpi_dp_new_table("DAAD");
	if (!aad)
		return log_msg_ret("aad", -ENOMEM);

	acpi_dp_add_integer_from_dt(dev, aad, "dlg,btn-cfg");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,mic-det-thr");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,jack-ins-deb");
	acpi_dp_add_string_from_dt(dev, aad, "dlg,jack-det-rate");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,jack-rem-deb");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,a-d-btn-thr");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,d-b-btn-thr");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,b-c-btn-thr");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,c-mic-btn-thr");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,btn-avg");
	acpi_dp_add_integer_from_dt(dev, aad, "dlg,adc-1bit-rpt");
	if (!dev_read_u32(dev, "dlg,micbias-pulse-lvl", &val)) {
		acpi_dp_add_integer_from_dt(dev, aad, "dlg,micbias-pulse-lvl");
		acpi_dp_add_integer_from_dt(dev, aad, "dlg,micbias-pulse-time");
	}

	/* DA7219 Properties */
	dsd = acpi_dp_new_table("_DSD");
	if (!dsd)
		return log_msg_ret("dsd", -ENOMEM);
	acpi_dp_add_integer_from_dt(dev, dsd, "dlg,micbias-lvl");
	acpi_dp_add_string_from_dt(dev, dsd, "dlg,mic-amp-in-sel");
	acpi_dp_add_string_from_dt(dev, dsd, "dlg,mclk-name");
	acpi_dp_add_child(dsd, "da7219_aad", aad);

	/* Write Device Property Hierarchy */
	acpi_dp_write(dsd);

	acpigen_pop_len(); /* Device */
	acpigen_pop_len(); /* Scope */
	printf("\n\nda7219: %p\n", acpigen_get_current());

	return 0;
}

struct acpi_ops da7219_acpi_ops = {
	.fill_ssdt_generator	= da7219_acpi_fill_ssdt,
};

static const struct udevice_id da7219_ids[] = {
	{ .compatible = "dlg,da7219" },
	{ }
};

U_BOOT_DRIVER(da7219) = {
	.name		= "da7219",
	.id		= UCLASS_MISC,
	.of_match	= da7219_ids,
	acpi_ops_ptr(&da7219_acpi_ops)
};
