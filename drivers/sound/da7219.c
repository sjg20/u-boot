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

#define DA7219_ACPI_NAME	"DLG7"
#define DA7219_ACPI_HID		"DLGS7219"

static int da7219_acpi_fill_ssdt(struct udevice *dev, struct acpi_ctx *ctx)
{
	struct dm_i2c_chip *chip = dev_get_parent_platdata(dev);
	struct dm_i2c_bus *i2c_bus = dev_get_uclass_priv(dev_get_parent(dev));
	char scope[ACPI_DEVICE_PATH_MAX];
	char name[ACPI_DEVICE_NAME_MAX];
	struct acpi_dp *dsd, *aad;
	struct acpi_i2c i2c;
	struct irq req_irq;
	u32 val;
	int ret;

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

	memset(&i2c, '\0', sizeof(i2c));
	i2c.address = chip->chip_addr,
	i2c.mode_10bit = 0;
	i2c.speed = i2c_bus->speed_hz > 100000 ? I2C_SPEED_FAST :
		IC_SPEED_MODE_STANDARD;
	i2c.resource = scope;

	/* Resources */
	acpigen_write_name("_CRS");
	acpigen_write_resourcetemplate_header();
	acpi_device_write_i2c(&i2c);

	ret = irq_get_by_index(dev, 0, &req_irq);

	/* Use either Interrupt() or GpioInt() */
	if (!ret) {
		struct acpi_irq irq;

		memset(&irq, '\0', sizeof(irq));
		irq.pin = req_irq.id;
		irq.mode = ACPI_IRQ_EDGE_TRIGGERED;
		irq.polarity = ACPI_IRQ_ACTIVE_LOW;
		irq.shared = ACPI_IRQ_EXCLUSIVE;
		irq.wake = ACPI_IRQ_NO_WAKE;
		acpi_device_write_interrupt(&irq);
	} else {
		struct acpi_gpio gpio;
		struct gpio_desc req_gpio;

		ret = gpio_request_by_name(dev, "req-gpios", 0,
					   &req_gpio, GPIOD_IS_IN);
		if (ret)
			return log_msg_ret("irq", ret);

		memset(&gpio, '\0', sizeof(gpio));
		gpio.type = ACPI_GPIO_TYPE_IO;
		gpio.pull = ACPI_GPIO_PULL_DEFAULT;
		gpio.io_restrict = ACPI_GPIO_IO_RESTRICT_OUTPUT;
		gpio.polarity = ACPI_GPIO_ACTIVE_HIGH;
		gpio.pin_count = 1;
		gpio.pins[0] = pinctrl_get_pad_from_gpio(&req_gpio);
		acpi_device_write_gpio(&gpio);
	}
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
	if (dev_read_u32(dev, "dlg,micbias-pulse-lvl", &val)) {
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
