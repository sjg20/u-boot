// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/clock.h>
#include <linux/errno.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <fsl_esdhc_imx.h>
#include <miiphy.h>
#include <netdev.h>

#include "common.h"

DECLARE_GLOBAL_DATA_PTR;


void board_debug_uart_init(void)
{
	/* Done in SPL */
}

int dram_init(void)
{
	gd->ram_size = SZ_1G;

	return 0;
}

static iomux_v3_cfg_t const uart5_pads[] = {
	MX6_PAD_CSI0_DAT14__UART5_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT15__UART5_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_GPIO_9__GPIO1_IO09	  | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#ifdef CONFIG_FEC_MXC
static iomux_v3_cfg_t const enet_pads[] = {
	MX6_PAD_ENET_MDC__ENET_MDC              | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_MDIO__ENET_MDIO            | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_REF_CLK__ENET_TX_CLK    	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_TX_EN__ENET_TX_EN    	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_TXD0__ENET_TX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_TXD1__ENET_TX_DATA1   	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_KEY_ROW2__ENET_TX_DATA2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_KEY_ROW0__ENET_TX_DATA3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_GPIO_18__ENET_RX_CLK		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_CRS_DV__ENET_RX_EN    	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RXD0__ENET_RX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RXD1__ENET_RX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_KEY_COL2__ENET_RX_DATA2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_KEY_COL0__ENET_RX_DATA3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RX_ER__ENET_RX_ER    	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_KEY_ROW1__ENET_COL		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_KEY_COL3__ENET_CRS		| MUX_PAD_CTRL(ENET_PAD_CTRL),

	/* PHY reset */
	MX6_PAD_KEY_COL1__GPIO4_IO08		| MUX_PAD_CTRL(NO_PAD_CTRL),
};

#ifdef CONFIG_MXC_SPI
static iomux_v3_cfg_t const ecspi1_pads[] = {
	MX6_PAD_EIM_EB2__GPIO2_IO30 | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_EIM_D18__ECSPI1_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_EIM_D17__ECSPI1_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_EIM_D16__ECSPI1_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL),
};

int board_spi_cs_gpio(unsigned bus, unsigned cs)
{
	return (bus == 0 && cs == 0) ? (IMX_GPIO_NR(2, 30)) : -1;
}

static void setup_spi(void)
{
	SETUP_IOMUX_PADS(ecspi1_pads);
}
#endif

static void setup_iomux_enet(void)
{
	SETUP_IOMUX_PADS(enet_pads);

	/* Reset the 88e6061 PHY on the Salmon carrier board */
	gpio_direction_output(IMX_GPIO_NR(4, 8), 0);
	udelay(500);
	gpio_set_value(IMX_GPIO_NR(4, 8), 1);
}
#endif

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart5_pads, ARRAY_SIZE(uart5_pads));
	gpio_direction_output(IMX_GPIO_NR(1, 9), 1);
}

#if 0
#define MII_MMD_ACCESS_CTRL_REG		0xd
#define MII_MMD_ACCESS_ADDR_DATA_REG	0xe
#define MII_DBG_PORT_REG		0x1d
#define MII_DBG_PORT2_REG		0x1e

int fecmxc_mii_postcall(int phy)
{
	unsigned short val;

	/*
	 * Due to the i.MX6Q Armadillo2 board HW design,there is
	 * no 125Mhz clock input from SOC. In order to use RGMII,
	 * We need enable AR8031 ouput a 125MHz clk from CLK_25M
	 */
	miiphy_write("FEC", phy, MII_MMD_ACCESS_CTRL_REG, 0x7);
	miiphy_write("FEC", phy, MII_MMD_ACCESS_ADDR_DATA_REG, 0x8016);
	miiphy_write("FEC", phy, MII_MMD_ACCESS_CTRL_REG, 0x4007);
	miiphy_read("FEC", phy, MII_MMD_ACCESS_ADDR_DATA_REG, &val);
	val &= 0xffe3;
	val |= 0x18;
	miiphy_write("FEC", phy, MII_MMD_ACCESS_ADDR_DATA_REG, val);

	/* For the RGMII phy, we need enable tx clock delay */
	miiphy_write("FEC", phy, MII_DBG_PORT_REG, 0x5);
	miiphy_read("FEC", phy, MII_DBG_PORT2_REG, &val);
	val |= 0x0100;
	miiphy_write("FEC", phy, MII_DBG_PORT2_REG, val);

	miiphy_write("FEC", phy, MII_BMCR, 0xa100);

	return 0;
}
#endif
int board_eth_init(bd_t *bis)
{
	setup_iomux_enet();
	return cpu_eth_init(bis);
}

int board_early_init_f(void)
{
	setup_iomux_uart();
#ifdef CONFIG_MXC_SPI
	setup_spi();
#endif

	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	return 0;
}

int checkboard(void)
{
	puts("Board: SnapperMX6\n");

	return 0;
}
