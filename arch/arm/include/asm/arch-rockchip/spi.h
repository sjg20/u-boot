/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 */

#ifndef __ASM_ARCH_SPI_H
#define __ASM_ARCH_SPI_H

#include <clk.h>
#include <fdtdec.h>

struct rockchip_spi_priv {
	struct rockchip_spi *regs;
	struct clk clk;
	struct tiny_clk tiny_clk;
	unsigned int max_freq;
	unsigned int mode;
	ulong last_transaction_us;	/* Time of last transaction end */
	unsigned int speed_hz;
	unsigned int last_speed_hz;
	uint input_rate;
	s32 frequency;		/* Default clock frequency, -1 for none */
	fdt_addr_t base;
	uint deactivate_delay_us;	/* Delay to wait after deactivate */
	uint activate_delay_us;		/* Delay to wait after activate */
	/* RXFIFO overruns and TXFIFO underruns stop the master clock */
	bool master_manages_fifo;
};

#endif
