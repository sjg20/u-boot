// SPDX-License-Identifier: GPL-2.0
/*
 * Something to do with Interrupts, but I don't know what ITSS stands for
 *
 * Copyright (C) 2017 Intel Corporation.
 * Copyright (C) 2017 Siemens AG
 * Copyright 2019 Google LLC
 *
 * Taken from coreboot itss.c
 */

#include <common.h>
#include <dm.h>
#include <dt-structs.h>
#include <itss.h>
#include <p2sb.h>
#include <spl.h>
#include <asm/arch/itss.h>

struct apl_itss_platdata {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	/* Put this first since driver model will copy the data here */
	struct dtd_intel_apl_itss dtplat;
#endif
};

/* struct pmc_route - Routing for PMC to GPIO */
struct pmc_route {
	u32 pmc;
	u32 gpio;
};

struct apl_itss_priv {
	struct pmc_route *route;
	uint route_count;
};

static int apl_set_irq_polarity(struct udevice *dev, uint irq, bool active_low)
{
	u32 mask;
	uint reg;

	if (irq > ITSS_MAX_IRQ)
		return -EINVAL;

	reg = PCR_ITSS_IPC0_CONF + sizeof(uint32_t) * (irq / IRQS_PER_IPC);
	mask = 1 << (irq % IRQS_PER_IPC);

	pcr_clrsetbits32(dev, reg, mask, active_low ? mask : 0);

	return 0;
}

static int apl_route_pmc_gpio_gpe(struct udevice *dev, uint pmc_gpe_num)
{
	struct apl_itss_priv *priv = dev_get_priv(dev);
	struct pmc_route *route;
	int i;

	for (i = 0, route = priv->route; i < priv->route_count; i++, route++) {
		if (pmc_gpe_num == route->pmc)
			return route->gpio;
	}

	return -ENOENT;
}

static int apl_itss_ofdata_to_platdata(struct udevice *dev)
{
	struct apl_itss_priv *priv = dev_get_priv(dev);
	int ret;

#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct apl_itss_platdata *plat = dev_get_platdata(dev);
	struct dtd_intel_apl_itss *dtplat = &plat->dtplat;

	/*
	 * It would be nice to do this in the bind() method, but with
	 * of-platdata binding happens in the order that DM finds things in the
	 * linker list (i.e. alphabetical order by driver name). So the GPIO
	 * device may well be bound before its parent (p2sb), and this call
	 * will fail if p2sb is not bound yet.
	 *
	 * TODO(sjg@chromium.org): Add a parent pointer to child devices in dtoc
	 */
	ret = p2sb_set_port_id(dev, dtplat->intel_p2sb_port_id);
	if (ret)
		return log_msg_ret("Could not set port id", ret);
	priv->route = (struct pmc_route *)dtplat->intel_pmc_routes;
	priv->route_count = ARRAY_SIZE(dtplat->intel_pmc_routes) /
		 sizeof(struct pmc_route);
#else
	int size;

	size = dev_read_size(dev, "intel,pmc-routes");
	if (size < 0)
		return size;
	priv->route = malloc(size);
	if (!priv->route)
		return -ENOMEM;
	ret = dev_read_u32_array(dev, "intel,pmc-routes", (u32 *)priv->route,
				 size / sizeof(fdt32_t));
	if (ret)
		return log_msg_ret("Cannot read pmc-routes", ret);
	priv->route_count = size / sizeof(struct pmc_route);
#endif

	return 0;
}

static const struct itss_ops apl_itss_ops = {
	.route_pmc_gpio_gpe	= apl_route_pmc_gpio_gpe,
	.set_irq_polarity	= apl_set_irq_polarity,
};

static const struct udevice_id apl_itss_ids[] = {
	{ .compatible = "intel,apl-itss"},
	{ }
};

U_BOOT_DRIVER(apl_itss_drv) = {
	.name		= "intel_apl_itss",
	.id		= UCLASS_ITSS,
	.of_match	= apl_itss_ids,
	.ops		= &apl_itss_ops,
	.ofdata_to_platdata = apl_itss_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct apl_itss_platdata),
	.priv_auto_alloc_size = sizeof(struct apl_itss_priv),
};
