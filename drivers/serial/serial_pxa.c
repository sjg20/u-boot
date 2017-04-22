// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 *
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * Copyright (C) 1999 2000 2001 Erik Mouw (J.A.K.Mouw@its.tudelft.nl)
 *
 * Modified to add driver model (DM) support
 * (C) Copyright 2016 Marcel Ziswiler <marcel.ziswiler@toradex.com>
 */

#include <common.h>
#include <hang.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/regs-uart.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/platform_data/serial_pxa.h>
#include <linux/compiler.h>
#include <serial.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

static uint32_t pxa_uart_get_baud_divider(int baudrate)
{
	return 921600 / baudrate;
}

static void pxa_uart_toggle_clock(uint32_t uart_index, int enable)
{
	uint32_t clk_reg, clk_offset, reg;

	clk_reg = UART_CLK_REG;
	clk_offset = UART_CLK_BASE << uart_index;

	reg = readl(clk_reg);

	if (enable)
		reg |= clk_offset;
	else
		reg &= ~clk_offset;

	writel(reg, clk_reg);
}

/*
 * Enable clock and set baud rate, parity etc.
 */
void pxa_setbrg_common(struct pxa_uart_regs *uart_regs, int port, int baudrate)
{
	uint32_t divider = pxa_uart_get_baud_divider(baudrate);
	if (!divider)
		hang();


	pxa_uart_toggle_clock(port, 1);

	/* Disable interrupts and FIFOs */
	writel(0, &uart_regs->ier);
	writel(0, &uart_regs->fcr);

	/* Set baud rate */
	writel(LCR_WLS0 | LCR_WLS1 | LCR_DLAB, &uart_regs->lcr);
	writel(divider & 0xff, &uart_regs->dll);
	writel(divider >> 8, &uart_regs->dlh);
	writel(LCR_WLS0 | LCR_WLS1, &uart_regs->lcr);

	/* Enable UART */
	writel(IER_UUE, &uart_regs->ier);
}

static int pxa_serial_probe(struct udevice *dev)
{
	struct pxa_serial_platdata *plat = dev->platdata;

	pxa_setbrg_common((struct pxa_uart_regs *)plat->base, plat->port,
			  plat->baudrate);
	return 0;
}

static int pxa_serial_putc(struct udevice *dev, const char ch)
{
	struct pxa_serial_platdata *plat = dev->platdata;
	struct pxa_uart_regs *uart_regs = (struct pxa_uart_regs *)plat->base;

	/* Wait for last character to go. */
	if (!(readl(&uart_regs->lsr) & LSR_TEMT))
		return -EAGAIN;

	writel(ch, &uart_regs->thr);

	return 0;
}

static int pxa_serial_getc(struct udevice *dev)
{
	struct pxa_serial_platdata *plat = dev->platdata;
	struct pxa_uart_regs *uart_regs = (struct pxa_uart_regs *)plat->base;

	/* Wait for a character to arrive. */
	if (!(readl(&uart_regs->lsr) & LSR_DR))
		return -EAGAIN;

	return readl(&uart_regs->rbr) & 0xff;
}

int pxa_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct pxa_serial_platdata *plat = dev->platdata;
	struct pxa_uart_regs *uart_regs = (struct pxa_uart_regs *)plat->base;
	int port = plat->port;

	pxa_setbrg_common(uart_regs, port, baudrate);

	return 0;
}

static int pxa_serial_pending(struct udevice *dev, bool input)
{
	struct pxa_serial_platdata *plat = dev->platdata;
	struct pxa_uart_regs *uart_regs = (struct pxa_uart_regs *)plat->base;

	if (input)
		return readl(&uart_regs->lsr) & LSR_DR ? 1 : 0;
	else
		return readl(&uart_regs->lsr) & LSR_TEMT ? 0 : 1;

	return 0;
}

static const struct dm_serial_ops pxa_serial_ops = {
	.putc		= pxa_serial_putc,
	.pending	= pxa_serial_pending,
	.getc		= pxa_serial_getc,
	.setbrg		= pxa_serial_setbrg,
};

U_BOOT_DRIVER(serial_pxa) = {
	.name	= "serial_pxa",
	.id	= UCLASS_SERIAL,
	.probe	= pxa_serial_probe,
	.ops	= &pxa_serial_ops,
	.flags	= DM_FLAG_PRE_RELOC,
};
