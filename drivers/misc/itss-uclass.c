// SPDX-License-Identifier: GPL-2.0
/*
 * ITSS is a type of interrupt controller used on recent Intel SoC.
 *
 * Copyright 2019 Google LLC
 */

#include <dm.h>
#include <itss.h>

int itss_route_pmc_gpio_gpe(struct udevice *dev, uint pmc_gpe_num)
{
	const struct itss_ops *ops = itss_get_ops(dev);

	if (!ops->route_pmc_gpio_gpe)
		return -ENOSYS;

	return ops->route_pmc_gpio_gpe(dev, pmc_gpe_num);
}

int itss_set_irq_polarity(struct udevice *dev, uint irq, bool active_low)
{
	const struct itss_ops *ops = itss_get_ops(dev);

	if (!ops->set_irq_polarity)
		return -ENOSYS;

	return ops->set_irq_polarity(dev, irq, active_low);
}

int itss_snapshot_irq_polarities(struct udevice *dev)
{
	const struct itss_ops *ops = itss_get_ops(dev);

	if (!ops->snapshot_irq_polarities)
		return -ENOSYS;

	return ops->snapshot_irq_polarities(dev);
}

int itss_restore_irq_polarities(struct udevice *dev)
{
	const struct itss_ops *ops = itss_get_ops(dev);

	if (!ops->restore_irq_polarities)
		return -ENOSYS;

	return ops->restore_irq_polarities(dev);
}

UCLASS_DRIVER(itss) = {
	.id		= UCLASS_ITSS,
	.name		= "itss",
};
