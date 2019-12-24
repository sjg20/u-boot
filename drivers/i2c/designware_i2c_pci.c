// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2009
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 * Copyright 2019 Google Inc
 */

#include <common.h>
#include <acpi.h>
#include <dm.h>
#include <spl.h>
#include <asm/acpigen.h>
#include <asm/lpss.h>
#include "designware_i2c.h"

enum {
	VANILLA		= 0,	/* standard I2C with no tweaks */
	INTEL_APL,		/* Apollo Lake I2C */
};

/* BayTrail HCNT/LCNT/SDA hold time */
static struct dw_scl_sda_cfg byt_config = {
	.ss_hcnt = 0x200,
	.fs_hcnt = 0x55,
	.ss_lcnt = 0x200,
	.fs_lcnt = 0x99,
	.sda_hold = 0x6,
};

/* Have a weak function for now - possibly should be a new uclass */
__weak void lpss_reset_release(void *regs);

static int designware_i2c_pci_ofdata_to_platdata(struct udevice *dev)
{
	struct dw_i2c *priv = dev_get_priv(dev);

	if (spl_phase() < PHASE_SPL) {
		u32 base;
		int ret;

		ret = dev_read_u32(dev, "early-regs", &base);
		if (ret)
			return log_msg_ret("early-regs", ret);

		/* Set i2c base address */
		dm_pci_write_config32(dev, PCI_BASE_ADDRESS_0, base);

		/* Enable memory access and bus master */
		dm_pci_write_config32(dev, PCI_COMMAND, PCI_COMMAND_MEMORY |
				      PCI_COMMAND_MASTER);
	}

	if (spl_phase() < PHASE_BOARD_F) {
		/* Handle early, fixed mapping into a different address space */
		priv->regs = (struct i2c_regs *)dm_pci_read_bar32(dev, 0);
	} else {
		priv->regs = (struct i2c_regs *)
			dm_pci_map_bar(dev, PCI_BASE_ADDRESS_0, PCI_REGION_MEM);
	}
	if (!priv->regs)
		return -EINVAL;

	/* Save base address from PCI BAR */
	if (IS_ENABLED(CONFIG_INTEL_BAYTRAIL))
		/* Use BayTrail specific timing values */
		priv->scl_sda_cfg = &byt_config;
	if (dev_get_driver_data(dev) == INTEL_APL)
		priv->has_spk_cnt = true;

	return designware_i2c_ofdata_to_platdata(dev);
}

static int designware_i2c_pci_probe(struct udevice *dev)
{
	struct dw_i2c *priv = dev_get_priv(dev);

	if (dev_get_driver_data(dev) == INTEL_APL) {
		/* Ensure controller is in D0 state */
		lpss_set_power_state(dev, STATE_D0);

		lpss_reset_release(priv->regs);
	}

	return designware_i2c_probe(dev);
}

static int designware_i2c_pci_bind(struct udevice *dev)
{
	char name[20];

	/*
	 * Create a unique device name for PCI type devices
	 * ToDo:
	 * Setting req_seq in the driver is probably not recommended.
	 * But without a DT alias the number is not configured. And
	 * using this driver is impossible for PCIe I2C devices.
	 * This can be removed, once a better (correct) way for this
	 * is found and implemented.
	 *
	 * TODO(sjg@chromium.org): Perhaps if uclasses had platdata this would
	 * be possible. We cannot use static data in drivers since they may be
	 * used in SPL or before relocation.
	 */
	dev->req_seq = gd->arch.dw_i2c_num_cards++;
	sprintf(name, "i2c_designware#%u", dev->req_seq);
	device_set_name(dev, name);

	return 0;
}

/*
 * Write ACPI object to describe speed configuration.
 *
 * ACPI Object: Name ("xxxx", Package () { scl_lcnt, scl_hcnt, sda_hold }
 *
 * SSCN: I2C_SPEED_STANDARD
 * FMCN: I2C_SPEED_FAST
 * FPCN: I2C_SPEED_FAST_PLUS
 * HSCN: I2C_SPEED_HIGH
 */
static void dw_i2c_acpi_write_speed_config(struct dw_i2c *priv)
{
	struct dw_i2c_speed_config *config = &priv->config;

	if (!config->scl_lcnt && !config->scl_hcnt && !config->sda_hold)
		return;

	if (priv->speed_mode >= IC_SPEED_MODE_HIGH)
		acpigen_write_name("HSCN");
	else if (priv->speed_mode >= IC_SPEED_FAST_PLUS)
		acpigen_write_name("FPCN");
	else if (priv->speed_mode >= IC_SPEED_MODE_FAST)
		acpigen_write_name("FMCN");
	else
		acpigen_write_name("SSCN");

	/* Package () { scl_lcnt, scl_hcnt, sda_hold } */
	acpigen_write_package(3);
	acpigen_write_word(config->scl_hcnt);
	acpigen_write_word(config->scl_lcnt);
	acpigen_write_dword(config->sda_hold);
	acpigen_pop_len();
}

/*
 * Generate I2C timing information into the SSDT for the OS driver to consume,
 * optionally applying override values provided by the caller.
 */
static void dw_i2c_acpi_fill_ssdt(struct udevice *dev, struct acpi_ctx *ctx)
{
	const struct dw_i2c_bus_config *bcfg;
	uintptr_t dw_i2c_addr;
	struct dw_i2c_speed_config sgen;
	int bus;
	const char *path;
	unsigned int speed;

	path = acpi_device_path(dev);
	if (!path)
		return;

	if (!device_active(dev))
		return;

	/* Ensure a default speed is available */
	speed = (bcfg->speed == 0) ? I2C_SPEED_FAST : bcfg->speed;

			ret = dw_i2c_calc_timing(priv, i2c_spd, bus_clk, spk_cnt,
					 &config);

	/* Report timing values for the OS driver */
	if (dw_i2c_gen_speed_config(dw_i2c_addr, speed, bcfg, &sgen) >= 0) {
		acpigen_write_scope(path);
		dw_i2c_acpi_write_speed_config(&sgen);
		acpigen_pop_len();
	}
}

struct acpi_ops dw_i2c_acpi_ops = {
	.fill_ssdt_generator	= dw_i2c_acpi_fill_ssdt,
};

static const struct udevice_id designware_i2c_pci_ids[] = {
	{ .compatible = "snps,designware-i2c-pci" },
	{ .compatible = "intel,apl-i2c", .data = INTEL_APL },
	{ }
};

U_BOOT_DRIVER(i2c_designware_pci) = {
	.name	= "i2c_designware_pci",
	.id	= UCLASS_I2C,
	.of_match = designware_i2c_pci_ids,
	.bind	= designware_i2c_pci_bind,
	.ofdata_to_platdata	= designware_i2c_pci_ofdata_to_platdata,
	.probe	= designware_i2c_pci_probe,
	.priv_auto_alloc_size = sizeof(struct dw_i2c),
	.remove = designware_i2c_remove,
	.flags = DM_FLAG_OS_PREPARE,
	.ops	= &designware_i2c_ops,
	acpi_ops_ptr(&dw_i2c_acpi_ops)
};

static struct pci_device_id designware_pci_supported[] = {
	/* Intel BayTrail has 7 I2C controller located on the PCI bus */
	{ PCI_VDEVICE(INTEL, 0x0f41) },
	{ PCI_VDEVICE(INTEL, 0x0f42) },
	{ PCI_VDEVICE(INTEL, 0x0f43) },
	{ PCI_VDEVICE(INTEL, 0x0f44) },
	{ PCI_VDEVICE(INTEL, 0x0f45) },
	{ PCI_VDEVICE(INTEL, 0x0f46) },
	{ PCI_VDEVICE(INTEL, 0x0f47) },
	{ PCI_VDEVICE(INTEL, 0x5aac), .driver_data = INTEL_APL },
	{ PCI_VDEVICE(INTEL, 0x5aae), .driver_data = INTEL_APL },
	{ PCI_VDEVICE(INTEL, 0x5ab0), .driver_data = INTEL_APL },
	{ PCI_VDEVICE(INTEL, 0x5ab2), .driver_data = INTEL_APL },
	{ PCI_VDEVICE(INTEL, 0x5ab4), .driver_data = INTEL_APL },
	{ PCI_VDEVICE(INTEL, 0x5ab6), .driver_data = INTEL_APL },
	{},
};

U_BOOT_PCI_DEVICE(i2c_designware_pci, designware_pci_supported);
