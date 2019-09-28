// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Intel Corp.
 * Copyright 2019 Google LLC
 *
 * Taken partly from coreboot gpio.c
 */

#define LOG_CATEGORY UCLASS_GPIO

#include <common.h>
#include <dm.h>
#include <dt-structs.h>
#include <itss.h>
#include <p2sb.h>
#include <spl.h>
#include <asm-generic/gpio.h>
#include <asm/arch/gpio.h>
#include <asm/arch/gpio_defs.h>
#include <asm/arch/itss.h>

/**
 * struct apl_gpio_platdata - platform data for each device
 *
 * @dtplat: of-platdata data from C struct
 * @num_cfgs: Number of configuration words for each pad
 * @comm: Pad community for this device
 */
struct apl_gpio_platdata {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	/* Put this first since driver model will copy the data here */
	struct dtd_intel_apl_gpio dtplat;
#endif
	int num_cfgs;
	const struct pad_community *comm;
};

/** struct apl_gpio_priv - private data for each device
 *
 * @itss: ITSS device (for interrupt handling)
 * @itss_pol_cfg: Use to program Interrupt Polarity Control (IPCx) register
 *	Each bit represents IRQx Active High Polarity Disable configuration:
 *	when set to 1, the interrupt polarity associated with IRQx is inverted
 *	to appear as Active Low to IOAPIC and vice versa
 */
struct apl_gpio_priv {
	struct udevice *itss;
	bool itss_pol_cfg;
};

#define GPIO_DWx_SIZE(x)	(sizeof(u32) * (x))
#define PAD_CFG_OFFSET(x, dw_num)	((x) + GPIO_DWx_SIZE(dw_num))
#define PAD_CFG0_OFFSET(x)	PAD_CFG_OFFSET(x, 0)
#define PAD_CFG1_OFFSET(x)	PAD_CFG_OFFSET(x, 1)

#define MISCCFG_GPE0_DW0_SHIFT 8
#define MISCCFG_GPE0_DW0_MASK (0xf << MISCCFG_GPE0_DW0_SHIFT)
#define MISCCFG_GPE0_DW1_SHIFT 12
#define MISCCFG_GPE0_DW1_MASK (0xf << MISCCFG_GPE0_DW1_SHIFT)
#define MISCCFG_GPE0_DW2_SHIFT 16
#define MISCCFG_GPE0_DW2_MASK (0xf << MISCCFG_GPE0_DW2_SHIFT)

#define GPI_SMI_STS_OFFSET(comm, group) ((comm)->gpi_smi_sts_reg_0 +	\
				((group) * sizeof(u32)))
#define GPI_SMI_EN_OFFSET(comm, group) ((comm)->gpi_smi_en_reg_0 +	\
				((group) * sizeof(u32)))
#define GPI_IS_OFFSET(comm, group) ((comm)->gpi_int_sts_reg_0 +	\
				((group) * sizeof(uint32_t)))
#define GPI_IE_OFFSET(comm, group) ((comm)->gpi_int_en_reg_0 +	\
				((group) * sizeof(uint32_t)))

static const struct reset_mapping rst_map[] = {
	{ .logical = PAD_CFG0_LOGICAL_RESET_PWROK, .chipset = 0U << 30 },
	{ .logical = PAD_CFG0_LOGICAL_RESET_DEEP, .chipset = 1U << 30 },
	{ .logical = PAD_CFG0_LOGICAL_RESET_PLTRST, .chipset = 2U << 30 },
};

static const struct pad_group apl_community_n_groups[] = {
	INTEL_GPP(N_OFFSET, N_OFFSET, GPIO_31),	/* NORTH 0 */
	INTEL_GPP(N_OFFSET, GPIO_32, JTAG_TRST_B),	/* NORTH 1 */
	INTEL_GPP(N_OFFSET, JTAG_TMS, SVID0_CLK),	/* NORTH 2 */
};

static const struct pad_group apl_community_w_groups[] = {
	INTEL_GPP(W_OFFSET, W_OFFSET, OSC_CLK_OUT_1),/* WEST 0 */
	INTEL_GPP(W_OFFSET, OSC_CLK_OUT_2, SUSPWRDNACK),/* WEST 1 */
};

static const struct pad_group apl_community_sw_groups[] = {
	INTEL_GPP(SW_OFFSET, SW_OFFSET, SMB_ALERTB),	/* SOUTHWEST 0 */
	INTEL_GPP(SW_OFFSET, SMB_CLK, LPC_FRAMEB),	/* SOUTHWEST 1 */
};

static const struct pad_group apl_community_nw_groups[] = {
	INTEL_GPP(NW_OFFSET, NW_OFFSET, PROCHOT_B),	/* NORTHWEST 0 */
	INTEL_GPP(NW_OFFSET, PMIC_I2C_SCL, GPIO_106),/* NORTHWEST 1 */
	INTEL_GPP(NW_OFFSET, GPIO_109, GPIO_123),	/* NORTHWEST 2 */
};

/* TODO(sjg@chromium.org): Consider moving this to device tree */
static const struct pad_community apl_gpio_communities[] = {
	{
		.port = PID_GPIO_N,
		.first_pad = N_OFFSET,
		.last_pad = SVID0_CLK,
		.num_gpi_regs = NUM_N_GPI_REGS,
		.gpi_status_offset = NUM_NW_GPI_REGS + NUM_W_GPI_REGS
			+ NUM_SW_GPI_REGS,
		.pad_cfg_base = PAD_CFG_BASE,
		.host_own_reg_0 = HOSTSW_OWN_REG_0,
		.gpi_int_sts_reg_0 = GPI_INT_STS_0,
		.gpi_int_en_reg_0 = GPI_INT_EN_0,
		.gpi_smi_sts_reg_0 = GPI_SMI_STS_0,
		.gpi_smi_en_reg_0 = GPI_SMI_EN_0,
		.max_pads_per_group = GPIO_MAX_NUM_PER_GROUP,
		.name = "GPIO_GPE_N",
		.acpi_path = "\\_SB.GPO0",
		.reset_map = rst_map,
		.num_reset_vals = ARRAY_SIZE(rst_map),
		.groups = apl_community_n_groups,
		.num_groups = ARRAY_SIZE(apl_community_n_groups),
	}, {
		.port = PID_GPIO_NW,
		.first_pad = NW_OFFSET,
		.last_pad = GPIO_123,
		.num_gpi_regs = NUM_NW_GPI_REGS,
		.gpi_status_offset = NUM_W_GPI_REGS + NUM_SW_GPI_REGS,
		.pad_cfg_base = PAD_CFG_BASE,
		.host_own_reg_0 = HOSTSW_OWN_REG_0,
		.gpi_int_sts_reg_0 = GPI_INT_STS_0,
		.gpi_int_en_reg_0 = GPI_INT_EN_0,
		.gpi_smi_sts_reg_0 = GPI_SMI_STS_0,
		.gpi_smi_en_reg_0 = GPI_SMI_EN_0,
		.max_pads_per_group = GPIO_MAX_NUM_PER_GROUP,
		.name = "GPIO_GPE_NW",
		.acpi_path = "\\_SB.GPO1",
		.reset_map = rst_map,
		.num_reset_vals = ARRAY_SIZE(rst_map),
		.groups = apl_community_nw_groups,
		.num_groups = ARRAY_SIZE(apl_community_nw_groups),
	}, {
		.port = PID_GPIO_W,
		.first_pad = W_OFFSET,
		.last_pad = SUSPWRDNACK,
		.num_gpi_regs = NUM_W_GPI_REGS,
		.gpi_status_offset = NUM_SW_GPI_REGS,
		.pad_cfg_base = PAD_CFG_BASE,
		.host_own_reg_0 = HOSTSW_OWN_REG_0,
		.gpi_int_sts_reg_0 = GPI_INT_STS_0,
		.gpi_int_en_reg_0 = GPI_INT_EN_0,
		.gpi_smi_sts_reg_0 = GPI_SMI_STS_0,
		.gpi_smi_en_reg_0 = GPI_SMI_EN_0,
		.max_pads_per_group = GPIO_MAX_NUM_PER_GROUP,
		.name = "GPIO_GPE_W",
		.acpi_path = "\\_SB.GPO2",
		.reset_map = rst_map,
		.num_reset_vals = ARRAY_SIZE(rst_map),
		.groups = apl_community_w_groups,
		.num_groups = ARRAY_SIZE(apl_community_w_groups),
	}, {
		.port = PID_GPIO_SW,
		.first_pad = SW_OFFSET,
		.last_pad = LPC_FRAMEB,
		.num_gpi_regs = NUM_SW_GPI_REGS,
		.gpi_status_offset = 0,
		.pad_cfg_base = PAD_CFG_BASE,
		.host_own_reg_0 = HOSTSW_OWN_REG_0,
		.gpi_int_sts_reg_0 = GPI_INT_STS_0,
		.gpi_int_en_reg_0 = GPI_INT_EN_0,
		.gpi_smi_sts_reg_0 = GPI_SMI_STS_0,
		.gpi_smi_en_reg_0 = GPI_SMI_EN_0,
		.max_pads_per_group = GPIO_MAX_NUM_PER_GROUP,
		.name = "GPIO_GPE_SW",
		.acpi_path = "\\_SB.GPO3",
		.reset_map = rst_map,
		.num_reset_vals = ARRAY_SIZE(rst_map),
		.groups = apl_community_sw_groups,
		.num_groups = ARRAY_SIZE(apl_community_sw_groups),
	},
};

static size_t relative_pad_in_comm(const struct pad_community *comm,
				   uint gpio)
{
	return gpio - comm->first_pad;
}

/* find the group within the community that the pad is a part of */
static int gpio_group_index(const struct pad_community *comm, uint relative_pad)
{
	int i;

	if (!comm->groups)
		return -ESPIPE;

	/* find the base pad number for this pad's group */
	for (i = 0; i < comm->num_groups; i++) {
		if (relative_pad >= comm->groups[i].first_pad &&
		    relative_pad < comm->groups[i].first_pad +
		    comm->groups[i].size)
			return i;
	}

	return -ENOENT;
}

static int gpio_group_index_scaled(const struct pad_community *comm,
				   uint relative_pad, size_t scale)
{
	int ret;

	ret = gpio_group_index(comm, relative_pad);
	if (ret < 0)
		return ret;

	return ret * scale;
}

static int gpio_within_group(const struct pad_community *comm,
			     uint relative_pad)
{
	int ret;

	ret = gpio_group_index(comm, relative_pad);
	if (ret < 0)
		return ret;

	return relative_pad - comm->groups[ret].first_pad;
}

static u32 gpio_bitmask_within_group(const struct pad_community *comm,
				     uint relative_pad)
{
	return 1U << gpio_within_group(comm, relative_pad);
}

/**
 * gpio_get_device() - Find the device for a particular pad
 *
 * Each GPIO device is attached to one community and this supports a number of
 * GPIO pins. This function finds the device which controls a particular pad.
 *
 * @pad: Pad to check
 * @devp: Returns the device for that pad
 * @return 0 if OK, -ENOTBLK if no device was found for the given pin
 */
static int gpio_get_device(uint pad, struct udevice **devp)
{
	struct udevice *dev;

	/*
	 * We have to probe each one of these since the community link is only
	 * attached in apl_gpio_ofdata_to_platdata().
	 */
	uclass_foreach_dev_probe(UCLASS_GPIO, dev) {
		struct apl_gpio_platdata *plat = dev_get_platdata(dev);
		const struct pad_community *comm = plat->comm;

		if (pad >= comm->first_pad && pad <= comm->last_pad) {
			*devp = dev;
			return 0;
		}
	}
	printf("pad %d not found\n", pad);

	return -ENOTBLK;
}

static int gpio_configure_owner(struct udevice *dev,
				const struct pad_config *cfg,
				const struct pad_community *comm)
{
	u32 hostsw_own;
	u16 hostsw_own_offset;
	int pin;
	int ret;

	pin = relative_pad_in_comm(comm, cfg->pad);

	/* Based on the gpio pin number configure the corresponding bit in
	 * HOSTSW_OWN register. Value of 0x1 indicates GPIO Driver onwership.
	 */
	hostsw_own_offset = comm->host_own_reg_0;
	ret = gpio_group_index_scaled(comm, pin, sizeof(u32));
	if (ret < 0)
		return ret;
	hostsw_own_offset += ret;

	hostsw_own = pcr_read32(dev, hostsw_own_offset);

	/* The 4th bit in pad_config 1 (RO) is used to indicate if the pad
	 * needs GPIO driver ownership.  Set the bit if GPIO driver ownership
	 * requested, otherwise clear the bit.
	 */
	if (cfg->pad_config[1] & PAD_CFG1_GPIO_DRIVER)
		hostsw_own |= gpio_bitmask_within_group(comm, pin);
	else
		hostsw_own &= ~gpio_bitmask_within_group(comm, pin);

	pcr_write32(dev, hostsw_own_offset, hostsw_own);

	return 0;
}

static int gpi_enable_smi(struct udevice *dev, const struct pad_config *cfg,
			  const struct pad_community *comm)
{
	u32 value;
	u16 sts_reg;
	u16 en_reg;
	int group;
	int pin;
	int ret;

	if ((cfg->pad_config[0] & PAD_CFG0_ROUTE_SMI) != PAD_CFG0_ROUTE_SMI)
		return 0;

	pin = relative_pad_in_comm(comm, cfg->pad);
	ret = gpio_group_index(comm, pin);
	if (ret < 0)
		return ret;
	group = ret;

	sts_reg = GPI_SMI_STS_OFFSET(comm, group);
	value = pcr_read32(dev, sts_reg);
	/* Write back 1 to reset the sts bits */
	pcr_write32(dev, sts_reg, value);

	/* Set enable bits */
	en_reg = GPI_SMI_EN_OFFSET(comm, group);
	pcr_setbits32(dev, en_reg, gpio_bitmask_within_group(comm, pin));

	return 0;
}

static int gpio_configure_itss(struct udevice *dev,
			       const struct pad_config *cfg,
			       uint pad_cfg_offset)
{
	struct apl_gpio_priv *priv = dev_get_priv(dev);

	if (!priv->itss_pol_cfg)
		return -ENOSYS;

	int irq;

	/* Set up ITSS polarity if pad is routed to APIC.
	 *
	 * The ITSS takes only active high interrupt signals. Therefore,
	 * if the pad configuration indicates an inversion assume the
	 * intent is for the ITSS polarity. Before forwarding on the
	 * request to the APIC there's an inversion setting for how the
	 * signal is forwarded to the APIC. Honor the inversion setting
	 * in the GPIO pad configuration so that a hardware active low
	 * signal looks that way to the APIC (double inversion).
	 */
	if (!(cfg->pad_config[0] & PAD_CFG0_ROUTE_IOAPIC))
		return 0;

	irq = pcr_read32(dev, PAD_CFG1_OFFSET(pad_cfg_offset));
	irq &= PAD_CFG1_IRQ_MASK;
	if (!irq) {
		log_err("GPIO %u doesn't support APIC routing\n", cfg->pad);

		return -EPROTONOSUPPORT;
	}
	itss_set_irq_polarity(priv->itss, irq,
			      cfg->pad_config[0] & PAD_CFG0_RX_POL_INVERT);

	return 0;
}

/* Number of DWx config registers can be different for different SOCs */
static uint pad_config_offset(const struct pad_community *comm, uint pad)
{
	size_t offset;

	offset = relative_pad_in_comm(comm, pad);
	offset *= GPIO_DWx_SIZE(GPIO_NUM_PAD_CFG_REGS);

	return offset + comm->pad_cfg_base;
}

static int gpio_pad_reset_config_override(const struct pad_community *comm,
					  u32 config_value)
{
	const struct reset_mapping *rst_map = comm->reset_map;
	int i;

	/* Logical reset values equal chipset values */
	if (!rst_map || !comm->num_reset_vals)
		return config_value;

	for (i = 0; i < comm->num_reset_vals; i++, rst_map++) {
		if ((config_value & PAD_CFG0_RESET_MASK) == rst_map->logical) {
			config_value &= ~PAD_CFG0_RESET_MASK;
			config_value |= rst_map->chipset;

			return config_value;
		}
	}
	log_err("Logical-to-Chipset mapping not found\n");

	return -ENOENT;
}

static const int mask[4] = {
	PAD_CFG0_TX_STATE |				\
	PAD_CFG0_TX_DISABLE | PAD_CFG0_RX_DISABLE | PAD_CFG0_MODE_MASK |
	PAD_CFG0_ROUTE_MASK | PAD_CFG0_RXTENCFG_MASK |
	PAD_CFG0_RXINV_MASK | PAD_CFG0_PREGFRXSEL |
	PAD_CFG0_TRIG_MASK | PAD_CFG0_RXRAW1_MASK |
	PAD_CFG0_RXPADSTSEL_MASK | PAD_CFG0_RESET_MASK,

#ifdef CONFIG_INTEL_GPIO_IOSTANDBY
	PAD_CFG1_IOSTERM_MASK | PAD_CFG1_PULL_MASK | PAD_CFG1_IOSSTATE_MASK,
#else
	PAD_CFG1_IOSTERM_MASK | PAD_CFG1_PULL_MASK,
#endif

	PAD_CFG2_DEBOUNCE_MASK,

	0,
};

/**
 * gpio_configure_pad() - Configure a pad
 *
 * @dev: GPIO device containing the pad (see gpio_get_device())
 * @cfg: Configuration to apply
 * @return 0 if OK, -ve on error
 */
static int gpio_configure_pad(struct udevice *dev, const struct pad_config *cfg)
{
	struct apl_gpio_platdata *plat = dev_get_platdata(dev);
	const struct pad_community *comm = plat->comm;
	uint config_offset;
	u32 pad_conf, soc_pad_conf;
	int ret;
	int i;

	if (IS_ERR(comm))
		return PTR_ERR(comm);
	config_offset = pad_config_offset(comm, cfg->pad);
	for (i = 0; i < GPIO_NUM_PAD_CFG_REGS; i++) {
		pad_conf = pcr_read32(dev, PAD_CFG_OFFSET(config_offset, i));

		soc_pad_conf = cfg->pad_config[i];
		if (i == 0) {
			ret = gpio_pad_reset_config_override(comm,
							     soc_pad_conf);
			if (ret < 0)
				return ret;
			soc_pad_conf = ret;
		}
		soc_pad_conf &= mask[i];
		soc_pad_conf |= pad_conf & ~mask[i];

		log_debug("gpio_padcfg [0x%02x, %02zd] DW%d [0x%08x : 0x%08x : 0x%08x]\n",
			  comm->port, relative_pad_in_comm(comm, cfg->pad), i,
			  pad_conf,/* old value */
			  cfg->pad_config[i], /* value passed from gpio table */
			  soc_pad_conf); /*new value*/
		pcr_write32(dev, PAD_CFG_OFFSET(config_offset, i),
			    soc_pad_conf);
	}
	ret = gpio_configure_itss(dev, cfg, config_offset);
	if (ret && ret != -ENOSYS)
		return log_msg_ret("itss config failed", ret);
	ret = gpio_configure_owner(dev, cfg, comm);
	if (ret)
		return ret;
	ret = gpi_enable_smi(dev, cfg, comm);
	if (ret)
		return ret;

	return 0;
}

static u32 get_config_reg_addr(struct udevice *dev, uint offset)
{
	struct apl_gpio_platdata *plat = dev_get_platdata(dev);
	const struct pad_community *comm = plat->comm;
	uint config_offset;

	config_offset = comm->pad_cfg_base + offset *
		 GPIO_DWx_SIZE(GPIO_NUM_PAD_CFG_REGS);

	return config_offset;
}

static u32 get_config_reg(struct udevice *dev, uint offset)
{
	uint config_offset = get_config_reg_addr(dev, offset);

	return pcr_read32(dev, config_offset);
}

static int apl_gpio_direction_input(struct udevice *dev, uint offset)
{
	uint config_offset = get_config_reg_addr(dev, offset);

	pcr_clrsetbits32(dev, config_offset,
			 PAD_CFG0_MODE_MASK | PAD_CFG0_TX_STATE |
				  PAD_CFG0_RX_DISABLE,
			 PAD_CFG0_MODE_GPIO | PAD_CFG0_TX_DISABLE);

	return 0;
}

static int apl_gpio_direction_output(struct udevice *dev, uint offset,
				     int value)
{
	uint config_offset = get_config_reg_addr(dev, offset);

	pcr_clrsetbits32(dev, config_offset,
			 PAD_CFG0_MODE_MASK | PAD_CFG0_RX_STATE |
				  PAD_CFG0_TX_DISABLE,
			 PAD_CFG0_MODE_GPIO | PAD_CFG0_RX_DISABLE |
				  (value ? PAD_CFG0_TX_STATE : 0));

	return 0;
}

static int apl_gpio_get_function(struct udevice *dev, uint offset)
{
	uint mode, rx_tx;
	u32 reg;

	reg = get_config_reg(dev, offset);
	mode = (reg & PAD_CFG0_MODE_MASK) >> PAD_CFG0_MODE_SHIFT;
	if (!mode) {
		rx_tx = reg & (PAD_CFG0_TX_DISABLE | PAD_CFG0_RX_DISABLE);
		if (rx_tx == PAD_CFG0_TX_DISABLE)
			return GPIOF_INPUT;
		else if (rx_tx == PAD_CFG0_RX_DISABLE)
			return GPIOF_OUTPUT;
	}

	return GPIOF_FUNC;
}

static int apl_gpio_get_value(struct udevice *dev, uint offset)
{
	uint mode, rx_tx;
	u32 reg;

	reg = get_config_reg(dev, offset);
	mode = (reg & PAD_CFG0_MODE_MASK) >> PAD_CFG0_MODE_SHIFT;
	if (!mode) {
		rx_tx = reg & (PAD_CFG0_TX_DISABLE | PAD_CFG0_RX_DISABLE);
		if (rx_tx == PAD_CFG0_TX_DISABLE)
			return mode & PAD_CFG0_RX_STATE_BIT ? 1 : 0;
		else if (rx_tx == PAD_CFG0_RX_DISABLE)
			return mode & PAD_CFG0_TX_STATE_BIT ? 1 : 0;
	}

	return 0;
}

int gpio_route_gpe(struct udevice *itss, uint gpe0b, uint gpe0c, uint gpe0d)
{
	struct udevice *gpio_dev;
	u32 misccfg_value;
	u32 misccfg_clr;
	int ret;

	/* Get the group here for community specific MISCCFG register.
	 * If any of these returns -1 then there is some error in devicetree
	 * where the group is probably hardcoded and does not comply with the
	 * PMC group defines. So we return from here and MISCFG is set to
	 * default.
	 */
	ret = itss_route_pmc_gpio_gpe(itss, gpe0b);
	if (ret)
		return ret;
	gpe0b = ret;

	ret = itss_route_pmc_gpio_gpe(itss, gpe0c);
	if (ret)
		return ret;
	gpe0c = ret;

	ret = itss_route_pmc_gpio_gpe(itss, gpe0d);
	if (ret)
		return ret;
	gpe0d = ret;

	misccfg_value = gpe0b << MISCCFG_GPE0_DW0_SHIFT;
	misccfg_value |= gpe0c << MISCCFG_GPE0_DW1_SHIFT;
	misccfg_value |= gpe0d << MISCCFG_GPE0_DW2_SHIFT;

	/* Program GPIO_MISCCFG */
	misccfg_clr = MISCCFG_GPE0_DW2_MASK | MISCCFG_GPE0_DW1_MASK |
		MISCCFG_GPE0_DW0_MASK;

	log_debug("misccfg_clr:%x misccfg_value:%x\n", misccfg_clr,
		  misccfg_value);
	uclass_foreach_dev_probe(UCLASS_GPIO, gpio_dev) {
		pcr_clrsetbits32(gpio_dev, GPIO_MISCCFG, misccfg_clr,
				 misccfg_value);
	}

	return 0;
}

int gpio_gpi_clear_int_cfg(void)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_GPIO, &uc);
	if (ret)
		return log_msg_ret("gpio uc", ret);
	uclass_foreach_dev(dev, uc) {
		struct apl_gpio_platdata *plat = dev_get_platdata(dev);
		const struct pad_community *comm = plat->comm;
		uint sts_value;
		int group;

		for (group = 0; group < comm->num_gpi_regs; group++) {
			/* Clear the enable register */
			pcr_write32(dev, GPI_IE_OFFSET(comm, group), 0);

			/* Read and clear the set status register bits*/
			sts_value = pcr_read32(dev,
					       GPI_IS_OFFSET(comm, group));
			pcr_write32(dev, GPI_IS_OFFSET(comm, group), sts_value);
		}
	}

	return 0;
}

int gpio_config_pads(struct udevice *dev, int num_cfgs, u32 *pads,
		     int pads_count)
{
	const u32 *ptr;
	int i;

	log_debug("%s: pads_count=%d\n", __func__, pads_count);
	for (ptr = pads, i = 0; i < pads_count; ptr += 1 + num_cfgs, i++) {
		struct udevice *pad_dev = NULL;
		struct pad_config *cfg;
		int ret;

		cfg = (struct pad_config *)ptr;
		ret = gpio_get_device(cfg->pad, &pad_dev);
		if (ret)
			return ret;
		ret = gpio_configure_pad(pad_dev, cfg);
		if (ret)
			return ret;
	}

	return 0;
}

static int apl_gpio_ofdata_to_platdata(struct udevice *dev)
{
	struct apl_gpio_platdata *plat = dev_get_platdata(dev);
	struct apl_gpio_priv *priv = dev_get_priv(dev);
	struct p2sb_child_platdata *pplat;
	int ret;
	int i;

	plat->num_cfgs = 2;
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	/*
	 * It would be nice to do this in the bind() method, but with
	 * of-platdata binding happens in the order that DM finds things in the
	 * linker list (i.e. alphabetical order by driver name). So the GPIO
	 * device may well be bound before its parent (p2sb), and this call
	 * will fail if p2sb is not bound yet.
	 *
	 * TODO(sjg@chromium.org): Add a parent pointer to child devices in dtoc
	 */
	ret = p2sb_set_port_id(dev, plat->dtplat.intel_p2sb_port_id);
	if (ret)
		return log_msg_ret("Could not set port id", ret);
#endif
	/* Attach this device to its community structure */
	pplat = dev_get_parent_platdata(dev);
	for (i = 0; i < ARRAY_SIZE(apl_gpio_communities); i++) {
		if (apl_gpio_communities[i].port == pplat->pid)
			plat->comm = &apl_gpio_communities[i];
	}
	if (!plat->comm) {
		log_err("Cannot find community for pid %d\n", pplat->pid);
		return -EDOM;
	}
	ret = uclass_first_device_err(UCLASS_ITSS, &priv->itss);
	if (ret)
		return log_msg_ret("Cannot find ITSS", ret);

	return 0;
}

static int apl_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *upriv = dev_get_uclass_priv(dev);
	struct apl_gpio_platdata *plat = dev_get_platdata(dev);
	struct apl_gpio_priv *priv = dev_get_priv(dev);
	const struct pad_community *comm = plat->comm;

	upriv->gpio_count = comm->last_pad - comm->first_pad + 1;
	upriv->bank_name = dev->name;
	priv->itss_pol_cfg = true;

	return 0;
}

static const struct dm_gpio_ops apl_gpio_ops = {
	.get_function	= apl_gpio_get_function,
	.get_value	= apl_gpio_get_value,
	.direction_input = apl_gpio_direction_input,
	.direction_output = apl_gpio_direction_output,
};

static const struct udevice_id apl_gpio_ids[] = {
	{ .compatible = "intel,apl-gpio"},
	{ }
};

U_BOOT_DRIVER(apl_gpio_drv) = {
	.name		= "intel_apl_gpio",
	.id		= UCLASS_GPIO,
	.of_match	= apl_gpio_ids,
	.probe		= apl_gpio_probe,
	.ops		= &apl_gpio_ops,
	.ofdata_to_platdata = apl_gpio_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct apl_gpio_priv),
	.platdata_auto_alloc_size = sizeof(struct apl_gpio_platdata),
};
