// SPDX-License-Identifier: GPL-2.0+
/*
 * spi driver for rockchip
 *
 * (C) 2019 Theobroma Systems Design und Consulting GmbH
 *
 * (C) Copyright 2015 Google, Inc
 *
 * (C) Copyright 2008-2013 Rockchip Electronics
 * Peter, Software Engineering, <superpeter.cai@gmail.com>.
 */

#define LOG_CATEGORY UCLASS_SPI

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dt-structs.h>
#include <errno.h>
#include <log.h>
#include <spi.h>
#include <time.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/periph.h>
#include <asm/arch-rockchip/spi.h>
#include <dm/pinctrl.h>
#include "rk_spi.h"

/* Change to 1 to output registers at the start of each transaction */
#define DEBUG_RK_SPI	0

/*
 * ctrlr1 is 16-bits, so we should support lengths of 0xffff + 1. However,
 * the controller seems to hang when given 0x10000, so stick with this for now.
 */
#define ROCKCHIP_SPI_MAX_TRANLEN		0xffff

enum rockchip_spi_type {
	RK_SPI_BASE,
	RK_SPI_RK33XX,
};

struct rockchip_spi_platdata {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_rockchip_rk3288_spi of_plat;
#endif
};

#define SPI_FIFO_DEPTH		32

static void rkspi_dump_regs(struct rockchip_spi *regs)
{
	debug("ctrl0: \t\t0x%08x\n", readl(&regs->ctrlr0));
	debug("ctrl1: \t\t0x%08x\n", readl(&regs->ctrlr1));
	debug("ssienr: \t\t0x%08x\n", readl(&regs->enr));
	debug("ser: \t\t0x%08x\n", readl(&regs->ser));
	debug("baudr: \t\t0x%08x\n", readl(&regs->baudr));
	debug("txftlr: \t\t0x%08x\n", readl(&regs->txftlr));
	debug("rxftlr: \t\t0x%08x\n", readl(&regs->rxftlr));
	debug("txflr: \t\t0x%08x\n", readl(&regs->txflr));
	debug("rxflr: \t\t0x%08x\n", readl(&regs->rxflr));
	debug("sr: \t\t0x%08x\n", readl(&regs->sr));
	debug("imr: \t\t0x%08x\n", readl(&regs->imr));
	debug("isr: \t\t0x%08x\n", readl(&regs->isr));
	debug("dmacr: \t\t0x%08x\n", readl(&regs->dmacr));
	debug("dmatdlr: \t0x%08x\n", readl(&regs->dmatdlr));
	debug("dmardlr: \t0x%08x\n", readl(&regs->dmardlr));
}

static void rkspi_enable_chip(struct rockchip_spi *regs, bool enable)
{
	writel(enable ? 1 : 0, &regs->enr);
}

static void rkspi_set_clk(struct rockchip_spi_priv *priv, uint speed)
{
	/*
	 * We should try not to exceed the speed requested by the caller:
	 * when selecting a divider, we need to make sure we round up.
	 */
	uint clk_div = DIV_ROUND_UP(priv->input_rate, speed);

	/* The baudrate register (BAUDR) is defined as a 32bit register where
	 * the upper 16bit are reserved and having 'Fsclk_out' in the lower
	 * 16bits with 'Fsclk_out' defined as follows:
	 *
	 *   Fsclk_out = Fspi_clk/ SCKDV
	 *   Where SCKDV is any even value between 2 and 65534.
	 */
	if (clk_div > 0xfffe) {
		clk_div = 0xfffe;
		debug("%s: can't divide down to %d Hz (actual will be %d Hz)\n",
		      __func__, speed, priv->input_rate / clk_div);
	}

	/* Round up to the next even 16bit number */
	clk_div = (clk_div + 1) & 0xfffe;

	log_debug("spi speed %u, div %u\n", speed, clk_div);

	clrsetbits_le32(&priv->regs->baudr, 0xffff, clk_div);
	priv->last_speed_hz = speed;
}

static int rkspi_wait_till_not_busy(struct rockchip_spi *regs)
{
	unsigned long start;

	start = get_timer(0);
	while (readl(&regs->sr) & SR_BUSY) {
		if (get_timer(start) > ROCKCHIP_SPI_TIMEOUT_MS) {
			debug("RK SPI: Status keeps busy for 1000us after a read/write!\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static void spi_cs_activate_bus(struct rockchip_spi_priv *priv, uint cs)
{
	struct rockchip_spi *regs = priv->regs;

	/* If it's too soon to do another transaction, wait */
	if (priv->deactivate_delay_us && priv->last_transaction_us) {
		ulong delay_us;		/* The delay completed so far */
		delay_us = timer_get_us() - priv->last_transaction_us;
		if (delay_us < priv->deactivate_delay_us) {
			ulong additional_delay_us =
				priv->deactivate_delay_us - delay_us;
			debug("%s: delaying by %ld us\n",
			      __func__, additional_delay_us);
			udelay(additional_delay_us);
		}
	}

	debug("activate cs%u\n", cs);
	writel(1 << cs, &regs->ser);
	if (priv->activate_delay_us)
		udelay(priv->activate_delay_us);
}

static void spi_cs_deactivate_bus(struct rockchip_spi_priv *priv, uint cs)
{
	struct rockchip_spi *regs = priv->regs;

	debug("deactivate cs%u\n", cs);
	writel(0, &regs->ser);

	/* Remember time of this transaction so we can honour the bus delay */
	if (priv->deactivate_delay_us)
		priv->last_transaction_us = timer_get_us();
}


static int rockchip_spi_calc_modclk(ulong max_freq)
{
	/*
	 * While this is not strictly correct for the RK3368, as the
	 * GPLL will be 576MHz, things will still work, as the
	 * clk_set_rate(...) implementation in our clock-driver will
	 * chose the next closest rate not exceeding what we request
	 * based on the output of this function.
	 */

	unsigned div;
	const unsigned long gpll_hz = 594000000UL;

	/*
	 * We need to find an input clock that provides at least twice
	 * the maximum frequency and can be generated from the assumed
	 * speed of GPLL (594MHz) using an integer divider.
	 *
	 * To give us more achievable bitrates at higher speeds (these
	 * are generated by dividing by an even 16-bit integer from
	 * this frequency), we try to have an input frequency of at
	 * least 4x our max_freq.
	 */

	div = DIV_ROUND_UP(gpll_hz, max_freq * 4);
	return gpll_hz / div;
}

static int rockchip_spi_probe_(struct rockchip_spi_priv *priv)
{
	int ret, rate;

	priv->regs = (struct rockchip_spi *)priv->base;

	priv->last_transaction_us = timer_get_us();
	priv->max_freq = priv->frequency;

	/* Clamp the value from the DTS against any hardware limits */
	if (priv->max_freq > ROCKCHIP_SPI_MAX_RATE)
		priv->max_freq = ROCKCHIP_SPI_MAX_RATE;

	/* Find a module-input clock that fits with the max_freq setting */
	log_debug("priv->max_freq=%d, modclk=%d\n", priv->max_freq,
		  rockchip_spi_calc_modclk(priv->max_freq));
	rate = rockchip_spi_calc_modclk(priv->max_freq);
	if (!CONFIG_IS_ENABLED(TINY_SPI)) {
		log_debug("clk=%s, id=%ld\n", priv->clk.dev->name,
			  priv->clk.id);
		ret = clk_set_rate(&priv->clk, rate);
	} else {
		log_debug("clk=%s, id=%ld\n", priv->tiny_clk.tdev->name,
			  priv->tiny_clk.id);
		ret = tiny_clk_set_rate(&priv->tiny_clk, rate);
	}
	if (ret < 0) {
		debug("%s: Failed to set clock: %d\n", __func__, ret);
		return log_ret(ret);
	}
	priv->input_rate = ret;
	debug("%s: rate = %u\n", __func__, priv->input_rate);

	return 0;
}

static int rockchip_spi_claim_bus_(struct rockchip_spi_priv *priv)
{
	struct rockchip_spi *regs = priv->regs;
	uint ctrlr0;

	/* Disable the SPI hardware */
	rkspi_enable_chip(regs, false);

	if (priv->speed_hz != priv->last_speed_hz)
		rkspi_set_clk(priv, priv->speed_hz);

	/* Operation Mode */
	ctrlr0 = OMOD_MASTER << OMOD_SHIFT;

	/* Data Frame Size */
	ctrlr0 |= DFS_8BIT << DFS_SHIFT;

	/* set SPI mode 0..3 */
	if (priv->mode & SPI_CPOL)
		ctrlr0 |= SCOL_HIGH << SCOL_SHIFT;
	if (priv->mode & SPI_CPHA)
		ctrlr0 |= SCPH_TOGSTA << SCPH_SHIFT;

	/* Chip Select Mode */
	ctrlr0 |= CSM_KEEP << CSM_SHIFT;

	/* SSN to Sclk_out delay */
	ctrlr0 |= SSN_DELAY_ONE << SSN_DELAY_SHIFT;

	/* Serial Endian Mode */
	ctrlr0 |= SEM_LITTLE << SEM_SHIFT;

	/* First Bit Mode */
	ctrlr0 |= FBM_MSB << FBM_SHIFT;

	/* Byte and Halfword Transform */
	ctrlr0 |= HALF_WORD_OFF << HALF_WORD_TX_SHIFT;

	/* Rxd Sample Delay */
	ctrlr0 |= 0 << RXDSD_SHIFT;

	/* Frame Format */
	ctrlr0 |= FRF_SPI << FRF_SHIFT;

	/* Tx and Rx mode */
	ctrlr0 |= TMOD_TR << TMOD_SHIFT;

	writel(ctrlr0, &regs->ctrlr0);

	return 0;
}

static int rockchip_spi_release_bus_(struct rockchip_spi_priv *priv)
{
	rkspi_enable_chip(priv->regs, false);

	return 0;
}

static int rockchip_spi_16bit_reader(struct rockchip_spi_priv *priv, u8 **din,
				     int *len)
{
	struct rockchip_spi *regs = priv->regs;
	const u32 saved_ctrlr0 = readl(&regs->ctrlr0);
#if defined(DEBUG)
	u32 statistics_rxlevels[33] = { };
#endif
	u32 frames = *len / 2;
	u8 *in = (u8 *)(*din);
	u32 max_chunk_size = SPI_FIFO_DEPTH;

	if (!frames)
		return 0;

	/*
	 * If we know that the hardware will manage RXFIFO overruns
	 * (i.e. stop the SPI clock until there's space in the FIFO),
	 * we the allow largest possible chunk size that can be
	 * represented in CTRLR1.
	 */
	if (priv->master_manages_fifo)
		max_chunk_size = ROCKCHIP_SPI_MAX_TRANLEN;

	rkspi_enable_chip(regs, false);
	clrsetbits_le32(&regs->ctrlr0,
			TMOD_MASK << TMOD_SHIFT,
			TMOD_RO << TMOD_SHIFT);
	/* 16bit data frame size */
	clrsetbits_le32(&regs->ctrlr0, DFS_MASK, DFS_16BIT);

	/* Update caller's context */
	const u32 bytes_to_process = 2 * frames;
	*din += bytes_to_process;
	*len -= bytes_to_process;

	/* Process our frames */
	while (frames) {
		u32 chunk_size = min(frames, max_chunk_size);

		log_debug("frames=%u\n", frames);
		frames -= chunk_size;

		writew(chunk_size - 1, &regs->ctrlr1);
		rkspi_enable_chip(regs, true);

		do {
			u32 rx_level = readw(&regs->rxflr);
#if defined(DEBUG)
			statistics_rxlevels[rx_level]++;
#endif
			chunk_size -= rx_level;
			while (rx_level--) {
				u16 val = readw(regs->rxdr);
				*in++ = val & 0xff;
				*in++ = val >> 8;
			}
			log_debug("chunk_size=%u\n", chunk_size);
		} while (chunk_size);

		rkspi_enable_chip(regs, false);
	}

#if defined(DEBUG)
	debug("%s: observed rx_level during processing:\n", __func__);
	for (int i = 0; i <= 32; ++i)
		if (statistics_rxlevels[i])
			debug("\t%2d: %d\n", i, statistics_rxlevels[i]);
#endif
	/* Restore the original transfer setup and return error-free. */
	writel(saved_ctrlr0, &regs->ctrlr0);

	return 0;
}

static int rockchip_spi_xfer_(struct rockchip_spi_priv *priv, uint bitlen,
			      const void *dout, void *din, ulong flags, uint cs)
{
	struct rockchip_spi *regs = priv->regs;
	int len = bitlen >> 3;
	const u8 *out = dout;
	u8 *in = din;
	int toread, towrite;
	int ret = 0;

	debug("%s: dout=%p, din=%p, len=%x, flags=%lx\n", __func__, dout, din,
	      len, flags);
	if (DEBUG_RK_SPI)
		rkspi_dump_regs(regs);

	/* Assert CS before transfer */
	if (flags & SPI_XFER_BEGIN)
		spi_cs_activate_bus(priv, cs);

	/*
	 * To ensure fast loading of firmware images (e.g. full U-Boot
	 * stage, ATF, Linux kernel) from SPI flash, we optimise the
	 * case of read-only transfers by using the full 16bits of each
	 * FIFO element.
	 */
	if (!out)
		ret = rockchip_spi_16bit_reader(priv, &in, &len);

	/* This is the original 8bit reader/writer code */
	while (len > 0) {
		int todo = min(len, ROCKCHIP_SPI_MAX_TRANLEN);

		log_debug("todo=%d\n", todo);
		rkspi_enable_chip(regs, false);
		writel(todo - 1, &regs->ctrlr1);
		rkspi_enable_chip(regs, true);

		toread = todo;
		towrite = todo;
		while (toread || towrite) {
			u32 status = readl(&regs->sr);

			if (towrite && !(status & SR_TF_FULL)) {
				writel(out ? *out++ : 0, regs->txdr);
				towrite--;
			}
			if (toread && !(status & SR_RF_EMPT)) {
				u32 byte = readl(regs->rxdr);

				if (in)
					*in++ = byte;
				toread--;
			}
		}

		/*
		 * In case that there's a transmit-component, we need to wait
		 * until the control goes idle before we can disable the SPI
		 * control logic (as this will implictly flush the FIFOs).
		 */
		if (out) {
			ret = rkspi_wait_till_not_busy(regs);
			if (ret)
				break;
		}

		len -= todo;
	}

	/* Deassert CS after transfer */
	if (flags & SPI_XFER_END)
		spi_cs_deactivate_bus(priv, cs);

	rkspi_enable_chip(regs, false);

	return ret;
}

#if !CONFIG_IS_ENABLED(TINY_SPI)
static int rockchip_spi_claim_bus(struct udevice *dev)
{
	struct udevice *bus = dev_get_parent(dev);
	struct rockchip_spi_priv *priv = dev_get_priv(bus);

	return rockchip_spi_claim_bus_(priv);
}

static int rockchip_spi_release_bus(struct udevice *dev)
{
	struct udevice *bus = dev_get_parent(dev);
	struct rockchip_spi_priv *priv = dev_get_priv(bus);

	rockchip_spi_release_bus_(priv);

	return 0;
}

static int rockchip_spi_xfer(struct udevice *dev, uint bitlen,
			     const void *dout, void *din, ulong flags)
{
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);
	struct udevice *bus = dev_get_parent(dev);
	struct rockchip_spi_priv *priv = dev_get_priv(bus);

	return rockchip_spi_xfer_(priv, bitlen, dout, din, flags,
				  slave_plat->cs);
}

static int rockchip_spi_set_speed(struct udevice *bus, uint speed)
{
	struct rockchip_spi_priv *priv = dev_get_priv(bus);

	/* Clamp to the maximum frequency specified in the DTS */
	if (speed > priv->max_freq)
		speed = priv->max_freq;

	priv->speed_hz = speed;

	return 0;
}

static int rockchip_spi_set_mode(struct udevice *bus, uint mode)
{
	struct rockchip_spi_priv *priv = dev_get_priv(bus);

	priv->mode = mode;

	return 0;
}

static int conv_of_platdata(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rockchip_spi_platdata *plat = dev->platdata;
	struct dtd_rockchip_rk3288_spi *dtplat = &plat->of_plat;
	struct rockchip_spi_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dtplat->reg[0];
	priv->frequency = 20000000;
	ret = clk_get_by_driver_info(dev, dtplat->clocks, &priv->clk);
	if (ret < 0)
		return log_ret(ret);
	dev->req_seq = 0;
#endif

	return 0;
}

static int rockchip_spi_probe(struct udevice *bus)
{
	struct rockchip_spi_priv *priv = dev_get_priv(bus);
	int ret;

	debug("%s: probe\n", __func__);
	if (CONFIG_IS_ENABLED(OF_PLATDATA)) {
		ret = conv_of_platdata(bus);
		if (ret)
			return log_ret(ret);
	}
	if (dev_get_driver_data(bus) == RK_SPI_RK33XX)
		priv->master_manages_fifo = true;

	return rockchip_spi_probe_(priv);
}

static int rockchip_spi_ofdata_to_platdata(struct udevice *bus)
{
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rockchip_spi_priv *priv = dev_get_priv(bus);
	int ret;

	priv->base = dev_read_addr(bus);

	ret = clk_get_by_index(bus, 0, &priv->clk);
	if (ret < 0) {
		debug("%s: Could not get clock for %s: %d\n", __func__,
		      bus->name, ret);
		return ret;
	}

	priv->frequency =
		dev_read_u32_default(bus, "spi-max-frequency", 50000000);
	priv->deactivate_delay_us =
		dev_read_u32_default(bus, "spi-deactivate-delay", 0);
	priv->activate_delay_us =
		dev_read_u32_default(bus, "spi-activate-delay", 0);

	debug("%s: base=%x, max-frequency=%d, deactivate_delay=%d\n",
	      __func__, (uint)priv->base, priv->frequency,
	      priv->deactivate_delay_us);
#endif

	return 0;
}

static const struct dm_spi_ops rockchip_spi_ops = {
	.claim_bus	= rockchip_spi_claim_bus,
	.release_bus	= rockchip_spi_release_bus,
	.xfer		= rockchip_spi_xfer,
	.set_speed	= rockchip_spi_set_speed,
	.set_mode	= rockchip_spi_set_mode,
	/*
	 * cs_info is not needed, since we require all chip selects to be
	 * in the device tree explicitly
	 */
};

static const struct udevice_id rockchip_spi_ids[] = {
	{ .compatible = "rockchip,rk3066-spi" },
	{ .compatible = "rockchip,rk3288-spi" },
	{ .compatible = "rockchip,rk3328-spi" },
	{ .compatible = "rockchip,rk3368-spi",
	  .data = RK_SPI_RK33XX },
	{ .compatible = "rockchip,rk3399-spi",
	  .data = RK_SPI_RK33XX },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3288_spi) = {
	.name	= "rockchip_rk3288_spi",
	.id	= UCLASS_SPI,
	.of_match = rockchip_spi_ids,
	.ops	= &rockchip_spi_ops,
	.ofdata_to_platdata = rockchip_spi_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct rockchip_spi_platdata),
	.priv_auto_alloc_size = sizeof(struct rockchip_spi_priv),
	.probe	= rockchip_spi_probe,
};

#else /* TINY_SPI */
static int rockchip_tiny_spi_claim_bus(struct tinydev *tdev)
{
	struct tinydev *tbus = tinydev_get_parent(tdev);
	struct rockchip_spi_priv *priv = tinydev_get_priv(tbus);

	return rockchip_spi_claim_bus_(priv);
}

static int rockchip_tiny_spi_release_bus(struct tinydev *tdev)
{
	struct tinydev *tbus = tinydev_get_parent(tdev);
	struct rockchip_spi_priv *priv = tinydev_get_priv(tbus);

	rockchip_spi_release_bus_(priv);

	return 0;
}

static int rockchip_tiny_set_speed_mode(struct tinydev *tbus, uint speed_hz,
					uint mode)
{
	struct rockchip_spi_priv *priv = tinydev_get_priv(tbus);

	/* Clamp to the maximum frequency specified in the DTS */
	if (speed_hz > priv->max_freq)
		speed_hz = priv->max_freq;

	priv->speed_hz = speed_hz;
	priv->mode = mode;

	return 0;
}

static int rockchip_tiny_spi_xfer(struct tinydev *tdev, uint bitlen,
				  const void *dout, void *din, ulong flags)
{
	log_debug("xfer\n");
	struct tinydev *tbus = tinydev_get_parent(tdev);
	struct rockchip_spi_priv *priv = tinydev_get_priv(tbus);
	struct dm_spi_slave_platdata *slave_plat;

	slave_plat = tinydev_get_data(tdev, DEVDATAT_PARENT_PLAT);
	log_debug("priv=%p, slave_plat=%p, cs=%d\n", priv, slave_plat,
		  slave_plat->cs);

	return rockchip_spi_xfer_(priv, bitlen, dout, din, flags,
				  slave_plat->cs);
}

static int rockchip_spi_tiny_probe(struct tinydev *tdev)
{
	log_debug("start\n");
	struct rockchip_spi_priv *priv = tinydev_get_priv(tdev);
	struct dtd_rockchip_rk3288_spi *dtplat = tdev->dtplat;
	int ret;

	priv->base = dtplat->reg[0];
	priv->frequency = 20000000;
	ret = tiny_clk_get_by_driver_info(dtplat->clocks, &priv->tiny_clk);
	if (ret < 0)
		return log_ret(ret);
	log_debug("priv->base=%lx\n", priv->base);

	return rockchip_spi_probe_(priv);
}

static struct tiny_spi_ops rockchip_spi_tiny_ops = {
	.claim_bus	= rockchip_tiny_spi_claim_bus,
	.release_bus	= rockchip_tiny_spi_release_bus,
	.set_speed_mode	= rockchip_tiny_set_speed_mode,
	.xfer		= rockchip_tiny_spi_xfer,
};

U_BOOT_TINY_DRIVER(rockchip_rk3288_spi) = {
	.uclass_id	= UCLASS_SPI,
	.probe		= rockchip_spi_tiny_probe,
	.ops		= &rockchip_spi_tiny_ops,
	DM_TINY_PRIV(<asm/arch-rockchip/spi.h>, \
		sizeof(struct rockchip_spi_priv))
};
#endif

U_BOOT_DRIVER_ALIAS(rockchip_rk3288_spi, rockchip_rk3368_spi)
