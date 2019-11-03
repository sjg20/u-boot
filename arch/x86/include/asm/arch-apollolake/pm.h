/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015-2016 Intel Corp.
 * (Written by Lance Zhao <lijian.zhao@intel.com> for Intel Corp.)
 */

#ifndef _ASM_ARCH_PM_H
#define _ASM_ARCH_PM_H

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

#endif
