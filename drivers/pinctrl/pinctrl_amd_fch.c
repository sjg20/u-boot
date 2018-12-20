// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <asm/arch/fch.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm/pinctrl.h>

struct fch_pinctrl_priv {
	u8 *regs;
};

/*
 * This table defines the IOMUX value required to configure a particular pin
 * as its GPIO function.
 */
static const uint8_t fch_gpio_use_table[FCH_NUM_GPIOS] = {
	/*         0   1   2   3   4   5   6   7   8   9 */
	/*   0 */  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/*  10 */  1,  2,  2,  1,  1,  1,  2,  2,  2,  2,
	/*  20 */  2,  1,  1,  2,  1,  1,  1,  0,  0,  0,
	/*  30 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
	/*  40 */  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,
	/*  50 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  60 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  70 */  0,  0,  0,  0,  1,  1,  0,  0,  0,  0,
	/*  80 */  0,  0,  0,  0,  1,  1,  1,  1,  1,  0,
	/*  90 */  0,  1,  3,  1,  0,  0,  0,  0,  0,  0,
	/* 100 */  0,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/* 110 */  0,  0,  0,  2,  2,  1,  1,  1,  1,  2,
	/* 120 */  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/* 130 */  1,  3,  2,  1,  1,  1,  1,  1,  1,  1,
	/* 140 */  1,  1,  1,  1,  1,  1,  1,  1,  1
};

static int amd_fch_pinmux_set(struct udevice *dev, uint pin_selector,
			      uint func_selector)
{
	struct fch_pinctrl_priv *priv = dev_get_priv(dev);

	switch (func_selector) {
	case PINCTRL_FCP_GPIO:
		writeb(fch_gpio_use_table[pin_selector],
		       &priv->regs[pin_selector]);
		break;
	default:
		return -ENOSYS;
	}

	return 0;
}

static int amd_fch_get_gpio_mux(struct udevice *dev, int banknum, int index)
{
	struct fch_pinctrl_priv *priv = dev_get_priv(dev);
	int val;

	if (banknum)
		return -EINVAL;
	val = readb(&priv->regs[index]);
	if (val == fch_gpio_use_table[index]) 
		return GPIOF_INPUT;

	return GPIOF_UNKNOWN;
}

static int amd_fch_pinctrl_probe(struct udevice *dev)
{
	struct fch_pinctrl_priv *priv = dev_get_priv(dev);

	priv->regs = dev_read_addr_ptr(dev);

	return 0;
}

const struct pinctrl_ops amd_fch_pinctrl_ops = {
	.get_gpio_mux = amd_fch_get_gpio_mux,
	.pinmux_set = amd_fch_pinmux_set,
};

static const struct udevice_id amd_fch_pinctrl_match[] = {
	{ .compatible = "amd,fch-pinctrl" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(amd_fch_pinctrl) = {
	.name = "amd_fch_pinctrl",
	.id = UCLASS_PINCTRL,
	.of_match = amd_fch_pinctrl_match,
	.ops = &amd_fch_pinctrl_ops,
	.probe = amd_fch_pinctrl_probe,
};
