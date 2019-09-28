// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <dt-structs.h>
#include <spl.h>
#include <asm/intel_regs.h>
#include <asm/pci.h>
#include <asm/arch/gpio.h>
#include <asm/arch/systemagent.h>

/**
 * struct apl_hostbridge_platdata - platform data for hostbridge
 *
 * @num_cfgs: Number of configuration words for each pad
 * @early_pads: Early pad data to set up, each (pad, cfg0, cfg1)
 * @early_pads_count: Number of pads to process
 * @pciex_region_size: BAR length in bytes
 * @bdf: Bus/device/function of hostbridge
 */
struct apl_hostbridge_platdata {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_intel_apl_hostbridge dtplat;
#endif
	int num_cfgs;
	u32 *early_pads;
	int early_pads_count;
	uint pciex_region_size;
	pci_dev_t bdf;
};

enum {
	PCIEXBAR		= 0x60,
	PCIEXBAR_LENGTH_256MB	= 0,
	PCIEXBAR_LENGTH_128MB,
	PCIEXBAR_LENGTH_64MB,

	PCIEXBAR_PCIEXBAREN	= 1 << 0,

	TSEG			= 0xb8,  /* TSEG base */
};

static int apl_hostbridge_early_init_gpio(struct udevice *dev)
{
	struct apl_hostbridge_platdata *plat = dev_get_platdata(dev);

	return gpio_config_pads(dev, plat->num_cfgs, plat->early_pads,
				plat->early_pads_count);
}

static int apl_hostbridge_early_init(struct udevice *dev)
{
	struct apl_hostbridge_platdata *plat = dev_get_platdata(dev);
	u32 region_size;
	ulong base;
	u32 reg;
	int ret;

	/* Set up the MCHBAR */
	pci_x86_read_config(plat->bdf, MCHBAR, &base, PCI_SIZE_32);
	base = MCH_BASE_ADDRESS;
	pci_x86_write_config(plat->bdf, MCHBAR, base | 1, PCI_SIZE_32);

	/*
	 * The PCIEXBAR is assumed to live in the memory mapped IO space under
	 * 4GiB
	 */
	pci_x86_write_config(plat->bdf, PCIEXBAR + 4, 0, PCI_SIZE_32);

	switch (plat->pciex_region_size >> 20) {
	default:
	case 256:
		region_size = PCIEXBAR_LENGTH_256MB;
		break;
	case 128:
		region_size = PCIEXBAR_LENGTH_128MB;
		break;
	case 64:
		region_size = PCIEXBAR_LENGTH_64MB;
		break;
	}

	reg = CONFIG_MMCONF_BASE_ADDRESS | (region_size << 1)
				| PCIEXBAR_PCIEXBAREN;
	pci_x86_write_config(plat->bdf, PCIEXBAR, reg, PCI_SIZE_32);

	/*
	 * TSEG defines the base of SMM range. BIOS determines the base
	 * of TSEG memory which must be at or below Graphics base of GTT
	 * Stolen memory, hence its better to clear TSEG register early
	 * to avoid power on default non-zero value (if any).
	 */
	pci_x86_write_config(plat->bdf, TSEG, 0, PCI_SIZE_32);

	ret = apl_hostbridge_early_init_gpio(dev);
	if (ret)
		return log_msg_ret("gpio", ret);

	return 0;
}

static int read_pads(ofnode node, const char *prop, int num_cfgs, u32 **padsp,
		     int *pad_countp)
{
	u32 *pads;
	int size;
	int ret;

	*padsp = NULL;
	*pad_countp = 0;
	size = ofnode_read_size(node, prop);
	if (size < 0)
		return 0;

	pads = malloc(size);
	if (!pads)
		return -ENOMEM;
	size /= sizeof(fdt32_t);
	ret = ofnode_read_u32_array(node, prop, pads, size);
	if (ret) {
		free(pads);
		return ret;
	}
	*pad_countp = size / (1 + num_cfgs);
	*padsp = pads;

	return 0;
}

int hostbridge_config_pads_for_node(struct udevice *dev, ofnode node)
{
	struct apl_hostbridge_platdata *plat = dev_get_platdata(dev);
	int pads_count;
	u32 *pads;
	int ret;

	ret = read_pads(node, "pads", plat->num_cfgs, &pads, &pads_count);
	if (ret)
		return log_msg_ret("no pads", ret);
	ret = gpio_config_pads(dev, plat->num_cfgs, pads, pads_count);
	free(pads);
	if (ret)
		return log_msg_ret("pad config", ret);

	return 0;
}

static int apl_hostbridge_ofdata_to_platdata(struct udevice *dev)
{
	struct apl_hostbridge_platdata *plat = dev_get_platdata(dev);

	plat->num_cfgs = 2;
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	int root;
	int ret;

	/* Get length of PCI Express Region */
	plat->pciex_region_size = dev_read_u32_default(dev, "pciex-region-size",
						       256 << 20);

	root = pci_x86_get_devfn(dev);
	if (root < 0)
		return log_msg_ret("Cannot get host-bridge PCI address", root);
	plat->bdf = root;
	ret = read_pads(dev_ofnode(dev), "early-pads", plat->num_cfgs,
			&plat->early_pads, &plat->early_pads_count);
	if (ret)
		return log_msg_ret("early-pads", ret);
#else
	struct dtd_intel_apl_hostbridge *dtplat = &plat->dtplat;
	int size;
	int i;

	plat->pciex_region_size = dtplat->pciex_region_size;
	plat->bdf = pci_x86_ofplat_get_devfn(dtplat->reg[0]);

	/* Assume that if everything is 0, it is empty */
	plat->early_pads = dtplat->early_pads;
	size = ARRAY_SIZE(dtplat->early_pads);
	for (i = 0; i < size;) {
		u32 val;
		int j;

		for (val = j = 0; j < plat->num_cfgs + 1; j++)
			val |= dtplat->early_pads[i + j];
		if (!val)
			break;
		plat->early_pads_count++;
		i += plat->num_cfgs + 1;
	}

#endif

	return 0;
}

static int apl_hostbridge_probe(struct udevice *dev)
{
	if (spl_phase() == PHASE_TPL)
		return apl_hostbridge_early_init(dev);

	return 0;
}

static const struct udevice_id apl_hostbridge_ids[] = {
	{ .compatible = "intel,apl-hostbridge" },
	{ }
};

U_BOOT_DRIVER(apl_hostbridge_drv) = {
	.name		= "intel_apl_hostbridge",
	.id		= UCLASS_NORTHBRIDGE,
	.of_match	= apl_hostbridge_ids,
	.ofdata_to_platdata = apl_hostbridge_ofdata_to_platdata,
	.probe		= apl_hostbridge_probe,
	.platdata_auto_alloc_size = sizeof(struct apl_hostbridge_platdata),
};
