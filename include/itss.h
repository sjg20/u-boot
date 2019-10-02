/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ITSS is a type of interrupt controller used on recent Intel SoC.
 *
 * Copyright 2019 Google LLC
 */

#ifndef __ITSS_H
#define __ITSS_H

/**
 * struct itss_ops - Operations for the ITSS
 */
struct itss_ops {
	/**
	 * route_pmc_gpio_gpe() - Get the GPIO for an event
	 *
	 * @dev: ITSS device
	 * @pmc_gpe_num: Event number to check
	 * @returns GPIO for the event, or -ENOENT if none
	 */
	int (*route_pmc_gpio_gpe)(struct udevice *dev, uint pmc_gpe_num);

	/**
	 * set_irq_polarity() - Set the IRQ polarity
	 *
	 * @dev: ITSS device
	 * @irq: Interrupt number to set
	 * @active_low: true if active low, false for active high
	 * @return 0 if OK, -EINVAL if @irq is invalid
	 */
	int (*set_irq_polarity)(struct udevice *dev, uint irq, bool active_low);

	/**
	 * snapshot_irq_polarities() - record IRQ polarities for later restore
	 *
	 * @dev: ITSS device
	 * @return 0
	 */
	int (*snapshot_irq_polarities)(struct udevice *dev);

	/**
	 * snapshot_irq_polarities() - restore IRQ polarities
	 *
	 * @dev: ITSS device
	 * @return 0
	 */
	int (*restore_irq_polarities)(struct udevice *dev);
};

#define itss_get_ops(dev)	((struct itss_ops *)(dev)->driver->ops)

/**
 * itss_route_pmc_gpio_gpe() - Get the GPIO for an event
 *
 * @dev: ITSS device
 * @pmc_gpe_num: Event number to check
 * @returns GPIO for the event, or -ENOENT if none
 */
int itss_route_pmc_gpio_gpe(struct udevice *dev, uint pmc_gpe_num);

/**
 * itss_set_irq_polarity() - Set the IRQ polarity
 *
 * @dev: ITSS device
 * @irq: Interrupt number to set
 * @active_low: true if active low, false for active high
 * @return 0 if OK, -EINVAL if @irq is invalid
 */
int itss_set_irq_polarity(struct udevice *dev, uint irq, bool active_low);

/**
 * itss_snapshot_irq_polarities() - record IRQ polarities for later restore
 *
 * @dev: ITSS device
 * @return 0
 */
int itss_snapshot_irq_polarities(struct udevice *dev);

/**
 * itss_snapshot_irq_polarities() - restore IRQ polarities
 *
 * @dev: ITSS device
 * @return 0
 */
int itss_restore_irq_polarities(struct udevice *dev);

#endif
