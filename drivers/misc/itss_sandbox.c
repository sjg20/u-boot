// SPDX-License-Identifier: GPL-2.0
/*
 * Sandbox driver for itss
 *
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <itss.h>

static int sandbox_set_irq_polarity(struct udevice *dev, uint irq,
				    bool active_low)
{
	if (irq > 10)
		return -EINVAL;

	return 0;
}

static int sandbox_route_pmc_gpio_gpe(struct udevice *dev, uint pmc_gpe_num)
{
	if (pmc_gpe_num > 10)
		return -ENOENT;

	return pmc_gpe_num + 1;
}

static const struct itss_ops sandbox_itss_ops = {
	.route_pmc_gpio_gpe	= sandbox_route_pmc_gpio_gpe,
	.set_irq_polarity	= sandbox_set_irq_polarity,
};

static const struct udevice_id sandbox_itss_ids[] = {
	{ .compatible = "sandbox,itss"},
	{ }
};

U_BOOT_DRIVER(sandbox_itss_drv) = {
	.name		= "sandbox_itss",
	.id		= UCLASS_ITSS,
	.of_match	= sandbox_itss_ids,
	.ops		= &sandbox_itss_ops,
};
