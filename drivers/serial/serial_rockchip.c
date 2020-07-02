// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 */

#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <dt-structs.h>
#include <log.h>
#include <ns16550.h>
#include <serial.h>
#include <asm/arch-rockchip/clock.h>

#if defined(CONFIG_ROCKCHIP_RK3188)
struct rockchip_uart_platdata {
	struct dtd_rockchip_rk3188_uart dtplat;
	struct ns16550_platdata plat;
};
struct dtd_rockchip_rk3188_uart *dtplat, s_dtplat;
#elif defined(CONFIG_ROCKCHIP_RK3288)
struct rockchip_uart_platdata {
	struct dtd_rockchip_rk3288_uart dtplat;
	struct ns16550_platdata plat;
};
struct dtd_rockchip_rk3288_uart *dtplat, s_dtplat;
#endif

#if !CONFIG_IS_ENABLED(TINY_SERIAL)
static int rockchip_serial_probe(struct udevice *dev)
{
	struct rockchip_uart_platdata *plat = dev_get_platdata(dev);

	/* Create some new platform data for the standard driver */
	plat->plat.base = plat->dtplat.reg[0];
	plat->plat.reg_shift = plat->dtplat.reg_shift;
	plat->plat.clock = plat->dtplat.clock_frequency;
	plat->plat.fcr = UART_FCR_DEFVAL;
	dev->platdata = &plat->plat;

	return ns16550_serial_probe(dev);
}

U_BOOT_DRIVER(rockchip_rk3188_uart) = {
	.name	= "rockchip_rk3188_uart",
	.id	= UCLASS_SERIAL,
	.priv_auto_alloc_size = sizeof(struct NS16550),
	.platdata_auto_alloc_size = sizeof(struct rockchip_uart_platdata),
	.probe	= rockchip_serial_probe,
	.ops	= &ns16550_serial_ops,
	.flags	= DM_FLAG_PRE_RELOC,
};

static const struct udevice_id rockchip_serial_ids[] = {
	{ .compatible = "rockchip,rk3288-uart" },
	{ },
};

U_BOOT_DRIVER(rockchip_rk3288_uart) = {
	.name	= "rockchip_rk3288_uart",
	.id	= UCLASS_SERIAL,
	.of_match = rockchip_serial_ids,
	.priv_auto_alloc_size = sizeof(struct NS16550),
	.platdata_auto_alloc_size = sizeof(struct rockchip_uart_platdata),
	.probe	= rockchip_serial_probe,
	.ops	= &ns16550_serial_ops,
	.flags	= DM_FLAG_PRE_RELOC,
};
#else /* TINY_SERIAL */

static int rockchip_serial_tiny_probe(struct tinydev *tdev)
{
	struct dtd_rockchip_rk3288_uart *dtplat = tdev->dtplat;
	struct ns16550_platdata *plat = tdev->priv;
	int ret;

	/* Create some new platform data for the standard driver */
	plat->base = dtplat->reg[0];
	plat->reg_shift = dtplat->reg_shift;
	plat->reg_width = dtplat->reg_io_width;
	plat->clock = dtplat->clock_frequency;
	plat->fcr = UART_FCR_DEFVAL;

	log_debug("plat=%p, base=%lx, offset=%x, width=%x, shift=%x, clock=%d, flags=%x\n",
		  plat, plat->base, plat->reg_offset, plat->reg_width,
		  plat->reg_shift, plat->clock, plat->flags);
	ret = ns16550_tiny_probe_plat(plat);
	if (ret)
		return log_ret(ret);

	return 0;
}

static int rockchip_serial_tiny_setbrg(struct tinydev *tdev, int baudrate)
{
	struct ns16550_platdata *plat = tdev->priv;

	return ns16550_tiny_setbrg(plat, baudrate);
}

static int rockchip_serial_tiny_putc(struct tinydev *tdev, const char ch)
{
	struct ns16550_platdata *plat = tdev->priv;

	return ns16550_tiny_putc(plat, ch);
}

struct tiny_serial_ops rockchip_serial_tiny_ops = {
	.setbrg	= rockchip_serial_tiny_setbrg,
	.putc	= rockchip_serial_tiny_putc,
};

U_BOOT_TINY_DRIVER(rockchip_rk3288_uart) = {
	.uclass_id	= UCLASS_SERIAL,
	.probe		= rockchip_serial_tiny_probe,
	.ops		= &rockchip_serial_tiny_ops,
	DM_TINY_PRIV(<ns16550.h>, sizeof(struct ns16550_platdata))
};
#endif
