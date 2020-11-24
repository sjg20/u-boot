/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 */

#ifndef _ASM_ARCH_UART_H
#define _ASM_ARCH_UART_H

#include <ns16550.h>

struct apl_ns16550_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_intel_apl_ns16550 dtplat;
#endif
	struct ns16550_plat ns16550;
};

/**
 * apl_uart_init() - Set up the APL UART device and clock
 *
 * This enables the PCI device, sets up the MMIO region and turns on the clock
 * using LPSS.
 *
 * The UART won't actually work unless the GPIO settings are correct and the
 * signals actually exit the SoC. See board_debug_uart_init() for that.
 */
void apl_uart_init(pci_dev_t bdf, ulong base);

#endif
