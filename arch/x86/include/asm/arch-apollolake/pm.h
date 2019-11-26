/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015-2016 Intel Corp.
 * (Written by Lance Zhao <lijian.zhao@intel.com> for Intel Corp.)
 */

#ifndef _ASM_ARCH_PM_H
#define _ASM_ARCH_PM_H

#include <power/acpi_pmc.h>

#define  PMC_GPE_SW_31_0	0
#define  PMC_GPE_SW_63_32	1
#define  PMC_GPE_NW_31_0	3
#define  PMC_GPE_NW_63_32	4
#define  PMC_GPE_NW_95_64	5
#define  PMC_GPE_N_31_0		6
#define  PMC_GPE_N_63_32	7
#define  PMC_GPE_W_31_0		9

#define IRQ_REG			0x106C
#define SCI_IRQ_ADJUST		24
#define SCI_IRQ_SEL		(255 << SCI_IRQ_ADJUST)
#define SCIS_IRQ9		9
#define SCIS_IRQ10		10
#define SCIS_IRQ11		11
#define SCIS_IRQ20		20
#define SCIS_IRQ21		21
#define SCIS_IRQ22		22
#define SCIS_IRQ23		23

/* P-state configuration */
#define PSS_MAX_ENTRIES		8
#define PSS_RATIO_STEP		2
#define PSS_LATENCY_TRANSITION	10
#define PSS_LATENCY_BUSMASTER	10

/* Track power state from reset to log events. */
struct __packed chipset_power_state {
	uint16_t pm1_sts;
	uint16_t pm1_en;
	uint32_t pm1_cnt;
	uint32_t gpe0_sts[GPE0_REG_MAX];
	uint32_t gpe0_en[GPE0_REG_MAX];
	uint16_t tco1_sts;
	uint16_t tco2_sts;
	uint32_t prsts;
	uint32_t gen_pmcon1;
	uint32_t gen_pmcon2;
	uint32_t gen_pmcon3;
	uint32_t prev_sleep_state;
};

#endif
