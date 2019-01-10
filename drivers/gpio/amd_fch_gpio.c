// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for AMD Stoney Ridge Fusion Controller Hub (FCH)
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <asm-generic/gpio.h>
#include <asm/io.h>
#include <asm/arch/fch.h>
#include <dt-bindings/gpio/amd-fch-gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include <dm/pinctrl.h>

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
	bool use_edge[FCH_NUM_GPIOS];
};

static int fch_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	u32 *regs = priv->regs;
	int ret;

	ret = pinctrl_pinmux_set(priv->pinctrl, offset, PINCTRL_FCP_GPIO);
	if (ret)
		return ret;
// 	printf("%s: old %x, \n", __func__, readl(&regs[offset]));
	clrbits_le32(&regs[offset], FCH_GPIO_OUTPUT_EN);
// 	printf("new %x\n", readl(&regs[offset]));

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

int fch_gpio_get_value_flags(struct udevice *dev, uint offset, ulong flags)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	u32 *regs = priv->regs;
	u32 val;

	val = readl(&regs[offset]);
	if (priv->use_edge[offset]) {
		if (val & FCH_GPIO_INTERRUPT_STS) {
			/* Clear interrupt status, preserve wake status */
			val &= ~FCH_GPIO_WAKE_STS;
			writel(val, &regs[offset]);
			return 1;
		}
		return 0;
	}

	return val & FCH_GPIO_INPUT_VAL ? 1 : 0;
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
	struct fch_gpio_priv *priv = dev_get_priv(dev);
	int mux;
	u32 val;

	mux = pinctrl_get_gpio_mux(priv->pinctrl, 0, offset);
	if (mux == GPIOF_INPUT) {
		val = readl(&priv->regs[offset]);
		if (val & FCH_GPIO_OUTPUT_EN)
			return GPIOF_OUTPUT;
		else
			return GPIOF_INPUT;
	}

	return GPIOF_UNKNOWN;
}

static int fch_gpio_xlate(struct udevice *dev, struct gpio_desc *desc,
			  struct ofnode_phandle_args *args)
{
	struct fch_gpio_priv *priv = dev_get_priv(dev);

	desc->offset = args->args[0];
	priv->use_edge[desc->offset] = false;
	desc->flags = 0;
	if (args->args[1] & GPIO_ACTIVE_LOW)
		desc->flags |= GPIOD_ACTIVE_LOW;
	if (args->args[1] & TRIGGER_EDGE) {
		desc->flags |= GPIOD_EDGE;
		priv->use_edge[desc->offset] = true;
	}

	return 0;
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
	.get_value_flags	= fch_gpio_get_value_flags,
	.set_value		= fch_gpio_set_value,
	.get_function		= fch_gpio_get_function,
	.xlate			= fch_gpio_xlate,
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
