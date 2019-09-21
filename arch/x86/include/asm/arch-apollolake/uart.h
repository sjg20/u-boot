/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 */

#ifndef __ASM_ARCH_UART_H
#define __ASM_ARCH_UART_H

/**
 * apl_uart_init() - Set up the APL UART device and clock
 *
 * The UART won't actually work unless the GPIO settings are correct and the
 * signals actually exit the SoC. See init_for_uart() for that.
 */
int apl_uart_init(pci_dev_t bdf, ulong base);

#endif
