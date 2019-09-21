/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 Intel Corporation.
 * Copyright 2019 Google LLC
 *
 * Modified from coreboot gpio.h
 */

#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H

/**
 * struct pad_config - config for a pad
 * @pad: offset of pad within community
 * @pad_config: Pad config data corresponding to DW0, DW1, etc.
 */
struct pad_config {
	int pad;
	u32 pad_config[4];
};

#include <asm/arch/gpio_apl.h>

/* GPIO community IOSF sideband clock gating */
#define MISCCFG_GPSIDEDPCGEN	BIT(5)
/* GPIO community RCOMP clock gating */
#define MISCCFG_GPRCOMPCDLCGEN	BIT(4)
/* GPIO community RTC clock gating */
#define MISCCFG_GPRTCDLCGEN	BIT(3)
/* GFX controller clock gating */
#define MISCCFG_GSXSLCGEN	BIT(2)
/* GPIO community partition clock gating */
#define MISCCFG_GPDPCGEN	BIT(1)
/* GPIO community local clock gating */
#define MISCCFG_GPDLCGEN	BIT(0)
/* Enable GPIO community power management configuration */
#define MISCCFG_ENABLE_GPIO_PM_CONFIG (MISCCFG_GPSIDEDPCGEN | \
	MISCCFG_GPRCOMPCDLCGEN | MISCCFG_GPRTCDLCGEN | MISCCFG_GSXSLCGEN \
	| MISCCFG_GPDPCGEN | MISCCFG_GPDLCGEN)

/*
 * GPIO numbers may not be contiguous and instead will have a different
 * starting pin number for each pad group.
 */
#define INTEL_GPP_BASE(first_of_community, start_of_group, end_of_group,\
			group_pad_base)					\
	{								\
		.first_pad = (start_of_group) - (first_of_community),	\
		.size = (end_of_group) - (start_of_group) + 1,		\
		.acpi_pad_base = (group_pad_base),			\
	}

/*
 * A pad base of -1 indicates that this group uses contiguous numbering
 * and a pad base should not be used for this group.
 */
#define PAD_BASE_NONE	-1

/* The common/default group numbering is contiguous */
#define INTEL_GPP(first_of_community, start_of_group, end_of_group)	\
	INTEL_GPP_BASE(first_of_community, start_of_group, end_of_group,\
		       PAD_BASE_NONE)

/**
 * struct reset_mapping - logical to actual value for PADRSTCFG in DW0
 *
 * Note that the values are expected to be within the field placement of the
 * register itself. i.e. if the reset field is at 31:30 then the values within
 * logical and chipset should occupy 31:30.
 */
struct reset_mapping {
	u32 logical;
	u32 chipset;
};

/**
 * struct pad_group - describes the groups within each community
 *
 * @first_pad: offset of first pad of the group relative to the community
 * @size: size of the group
 * @acpi_pad_base: starting pin number for the pads in this group when they are
 *	used in ACPI.  This is only needed if the pins are not contiguous across
 *	groups. Most groups will have this set to PAD_BASE_NONE and use
 *	contiguous numbering for ACPI.
 */
struct pad_group {
	int first_pad;
	uint size;
	int acpi_pad_base;
};

/**
 * struct pad_community - GPIO community
 *
 * This describes a community, or each group within a community when multiple
 * groups exist inside a community
 *
 * @name: Community name
 * @acpi_path: ACPI path
 * @num_gpi_regs: number of gpi registers in community
 * @max_pads_per_group: number of pads in each group; number of pads bit-mapped
 *	in each GPI status/en and Host Own Reg
 * @first_pad: first pad in community
 * @last_pad: last pad in community
 * @host_own_reg_0: offset to Host Ownership Reg 0
 * @gpi_int_sts_reg_0: offset to GPI Int STS Reg 0
 * @gpi_int_en_reg_0: offset to GPI Int Enable Reg 0
 * @gpi_smi_sts_reg_0: offset to GPI SMI STS Reg 0
 * @gpi_smi_en_reg_0: offset to GPI SMI EN Reg 0
 * @pad_cfg_base: offset to first PAD_GFG_DW0 Reg
 * @gpi_status_offset: specifies offset in struct gpi_status
 * @port: PCR Port ID
 * @reset_map: PADRSTCFG logical to chipset mapping
 * @num_reset_vals: number of values in @reset_map
 * @groups; list of groups for this community
 * @num_groups: number of groups
 */
struct pad_community {
	const char *name;
	const char *acpi_path;
	size_t num_gpi_regs;
	size_t max_pads_per_group;
	uint first_pad;
	uint last_pad;
	u16 host_own_reg_0;
	u16 gpi_int_sts_reg_0;
	u16 gpi_int_en_reg_0;
	u16 gpi_smi_sts_reg_0;
	u16 gpi_smi_en_reg_0;
	u16 pad_cfg_base;
	u8 gpi_status_offset;
	u8 port;
	const struct reset_mapping *reset_map;
	size_t num_reset_vals;
	const struct pad_group *groups;
	size_t num_groups;
};

/**
 * gpio_route_gpe() - set the GPIO groups for the general-purpose-event blocks
 *
 * The values from PMC register GPE_CFG are passed which is then mapped to
 * proper groups for MISCCFG. This basically sets the MISCCFG register bits:
 *  dw0 = gpe0_route[11:8]. This is ACPI GPE0b.
 *  dw1 = gpe0_route[15:12]. This is ACPI GPE0c.
 *  dw2 = gpe0_route[19:16]. This is ACPI GPE0d.
 *
 * @dev: ITSS device
 * @gpe0b: Value for GPE0B
 * @gpe0c: Value for GPE0C
 * @gpe0d: Value for GPE0D
 * @return 0 if OK, -ve on error
 */
int gpio_route_gpe(struct udevice *dev, uint gpe0b, uint gpe0c, uint gpe0d);

#endif
