/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for the GPIO subsystem on Tigerlake
 *
 * Taken from src/soc/intel/tigerlake/include/soc in coreboot
 */

#ifndef _ASM_ARCH_GPIO_H_
#define _ASM_ARCH_GPIO_H_

#ifdef CONFIG_SOC_INTEL_TIGERLAKE_PCH_H
#include <asm/arch/gpio_defs_pch_h.h>
#else
#include <asm/arch/gpio_defs.h>
#define CROS_GPIO_DEVICE_NAME	"INT34C5:00"
#endif
#include <asm/intel_pinctrl.h>

/* Enable GPIO community power management configuration */
#define MISCCFG_GPIO_PM_CONFIG_BITS (MISCCFG_GPVNNREQEN | \
	MISCCFG_GPPGCBDPCGEN | MISCCFG_GPSIDEDPCGEN | \
	MISCCFG_GPRCOMPCDLCGEN | MISCCFG_GPRTCDLCGEN | MISCCFG_GSXSLCGEN \
	| MISCCFG_GPDPCGEN | MISCCFG_GPDLCGEN)

#endif
