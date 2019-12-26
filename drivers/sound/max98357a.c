// SPDX-License-Identifier: GPL-2.0+
/*
 * max98357a.c -- MAX98357A Audio driver
 *
 * Copyright 2019 Google LLC
 * Parts taken from coreboot
 * (add to this if we take driver code from somewhere)
 */

#include <common.h>
#include <acpi.h>
#include <audio_codec.h>
#include <dm.h>
#include <sound.h>
#include <asm/acpigen.h>
#include <asm/acpi_device.h>
#include <asm/intel_pinctrl.h>
#include <asm-generic/gpio.h>

#define MAX98357A_ACPI_HID		"MX98357A"
#define MAX98357A_ACPI_FULL_NAME

static int max97357a_acpi_fill_ssdt(struct udevice *dev, struct acpi_ctx *ctx)
{
	char scope[ACPI_DEVICE_PATH_MAX];
	char name[ACPI_DEVICE_NAME_MAX];
	struct gpio_desc sdmode_gpio;
	struct acpi_gpio gpio;
	struct acpi_dp *dp;
	const char *path;
	int ret;

	ret = acpi_device_scope(dev, scope, sizeof(scope));
	if (ret)
		return log_msg_ret("scope", ret);
	ret = acpi_device_name(dev, name);
	if (ret)
		return log_msg_ret("name", ret);
	gpio_request_by_name(dev, "sdmode-gpios", 0, &sdmode_gpio, GPIOD_IS_IN);

	/* Device */
	acpigen_write_scope(scope);
	acpigen_write_device(name);
	acpigen_write_name_string("_HID", MAX98357A_ACPI_HID);
	acpigen_write_name_integer("_UID", 0);
	acpigen_write_name_string("_DDN", dev_read_string(dev, "acpi-ddn"));
	acpigen_write_sta(acpi_device_status(dev));

	memset(&gpio, '\0', sizeof(gpio));
	gpio.type = ACPI_GPIO_TYPE_IO;
	gpio.pull = ACPI_GPIO_PULL_DEFAULT;
	gpio.io_restrict = ACPI_GPIO_IO_RESTRICT_OUTPUT;
	gpio.polarity = ACPI_GPIO_ACTIVE_HIGH;
	gpio.pin_count = 1;
	gpio.pins[0] = pinctrl_get_pad_from_gpio(&sdmode_gpio);

	/* Resources */
	acpigen_write_name("_CRS");
	acpigen_write_resourcetemplate_header();
	acpi_device_write_gpio(&gpio);
	acpigen_write_resourcetemplate_footer();

	/* _DSD for devicetree properties */
	/* This points to the first pin in the first gpio entry in _CRS */
	path = acpi_device_path(dev);
	dp = acpi_dp_new_table("_DSD");
	acpi_dp_add_gpio(dp, "sdmode-gpio", path, 0, 0, gpio.polarity);
	acpi_dp_add_integer(dp, "sdmode-delay",
			    dev_read_u32_default(dev, "sdmode-delay", 0));
	acpi_dp_write(dp);

	acpigen_pop_len(); /* Device */
	acpigen_pop_len(); /* Scope */

	return 0;
}

struct acpi_ops max97357a_acpi_ops = {
	.fill_ssdt_generator	= max97357a_acpi_fill_ssdt,
};

static const struct audio_codec_ops max98357a_ops = {
};

static const struct udevice_id max98357a_ids[] = {
	{ .compatible = "maxim,max98357a" },
	{ }
};

U_BOOT_DRIVER(max98357a) = {
	.name		= "max98357a",
	.id		= UCLASS_AUDIO_CODEC,
	.of_match	= max98357a_ids,
	.ops		= &max98357a_ops,
	acpi_ops_ptr(&max97357a_acpi_ops)
};
