// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for AMD Stoney Ridge Fusion Controller Hub (FCH)
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/fcp.h>
#include <dm/pinctrl.h>

enum {
	FCH_NUM_GPIOS		= 149,
};

enum {
	FCH_GPIO_WAKE_STS	= 1 << 29,
	FCH_GPIO_INTERRUPT_STS	= 1 << 28,
	FCH_GPIO_OUTPUT_EN	= 1 << 23,
	FCH_GPIO_OUTPUT_VAL	= 1 << 22,
	FCH_GPIO_INPUT_VAL	= 1 << 16,
};

struct fch_gpio_priv {
	u32 *regs;
	struct udevice *pinctrl;
};

static int fch_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	u32 *regs = priv->regs;
	int ret;

	ret = pinctrl_pinmux_set(priv->pinctrl, offset, PINCTRL_FCP_GPIO);
	if (ret)
		return ret;
	clrbits_le32(&regs[offset], FCH_GPIO_OUTPUT_EN);

	return 0;
}

static int fch_gpio_direction_output(struct udevice *dev, unsigned offset,
				     int value)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	u32 *regs = priv->regs;
	int ret;

	ret = pinctrl_pinmux_set(priv->pinctrl, offset, PINCTRL_FCP_GPIO);
	if (ret)
		return ret;
	setbits_le32(&regs[offset], FCH_GPIO_OUTPUT_EN);

	return 0;
}

static int fch_gpio_get_value(struct udevice *dev, unsigned offset)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	u32 *regs = priv->regs;

	return readl(&regs[offset]) & FCH_GPIO_INPUT_VAL ? 1 : 0;
}

static int fch_gpio_set_value(struct udevice *dev, unsigned offset,
				   int value)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	u32 *regs = priv->regs;
	u32 mask = FCH_GPIO_OUTPUT_EN;

	clrsetbits_le32(&regs[offset], FCH_GPIO_OUTPUT_VAL,
			value ? mask | FCH_GPIO_OUTPUT_VAL : mask);

	return 0;
}

static int fch_gpio_get_function(struct udevice *dev, unsigned offset)
{
	return -ENODATA;
}

static int fch_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	int ret;

	priv->regs = dev_read_addr_ptr(dev);
	ret = uclass_first_device_err(UCLASS_PINCTRL, &priv->pinctrl);
	if (ret)
		return ret;

	uc_priv->gpio_count = FCH_NUM_GPIOS;
	uc_priv->bank_name = "a";

	return 0;
}

static const struct dm_gpio_ops gpio_fch_ops = {
	.direction_input	= fch_gpio_direction_input,
	.direction_output	= fch_gpio_direction_output,
	.get_value		= fch_gpio_get_value,
	.set_value		= fch_gpio_set_value,
	.get_function		= fch_gpio_get_function,
};

static const struct udevice_id fch_gpio_ids[] = {
	{ .compatible = "amd,fch-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_fch) = {
	.name	= "gpio_fch",
	.id	= UCLASS_GPIO,
	.of_match = fch_gpio_ids,
	.ops	= &gpio_fch_ops,
	.priv_auto_alloc_size = sizeof(struct fch_gpio_priv),
	.probe	= fch_gpio_probe,
};
