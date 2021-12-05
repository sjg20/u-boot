// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <debug_uart.h>

/*
 * board_debug_uart_init() - Init the debug UART ready for use
 *
 * This is the minimum init needed to get the UART running. It avoids any
 * drivers or complex code, so that the UART is running as soon as possible.
 */
void board_debug_uart_init(void)
{
}
