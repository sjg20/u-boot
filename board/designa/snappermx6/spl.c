// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 DesignA Electronics
 * Andre Renaud, DesignA Electronics, arenaud@designa-electronics.com
 *
 */

#include <common.h>
#include <cpu_func.h>
#include <env.h>
#include <serial.h>
#include <spl.h>
#include <linux/libfdt.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/mx6-ddr.h>
#include <asm/arch/mx6-pins.h>
#include "asm/arch/crm_regs.h"
#include <asm/arch/sys_proto.h>
#include <asm/arch/imx-regs.h>
#include "asm/arch/iomux.h"
#include <asm/mach-imx/iomux-v3.h>
#include <asm/gpio.h>
#include <fsl_esdhc_imx.h>
#include <netdev.h>
#include <bootcount.h>
#include <watchdog.h>
#include "common.h"

DECLARE_GLOBAL_DATA_PTR;

static const struct mx6dq_iomux_ddr_regs mx6_ddr_ioregs = {
	.dram_sdclk_0 = 0x00000030,
	.dram_sdclk_1 = 0x00000030,
	.dram_cas = 0x00000030,
	.dram_ras = 0x00000030,
	.dram_reset = 0x00000030,
	.dram_sdcke0 = 0x00003000,
	.dram_sdcke1 = 0x00003000,
	.dram_sdba2 = 0x00000000,
	.dram_sdodt0 = 0x00000030,
	.dram_sdodt1 = 0x00000030,

	.dram_sdqs0 = 0x00000030,
	.dram_sdqs1 = 0x00000030,
	.dram_sdqs2 = 0x00000030,
	.dram_sdqs3 = 0x00000030,
	.dram_sdqs4 = 0x00000030,
	.dram_sdqs5 = 0x00000030,
	.dram_sdqs6 = 0x00000030,
	.dram_sdqs7 = 0x00000030,

	.dram_dqm0 = 0x00000030,
	.dram_dqm1 = 0x00000030,
	.dram_dqm2 = 0x00000030,
	.dram_dqm3 = 0x00000030,
	.dram_dqm4 = 0x00000030,
	.dram_dqm5 = 0x00000030,
	.dram_dqm6 = 0x00000030,
	.dram_dqm7 = 0x00000030,
};

static const struct mx6dq_iomux_grp_regs mx6_grp_ioregs = {
	.grp_ddr_type = 0x000c0000,
	.grp_ddrmode_ctl = 0x00020000,
	.grp_ddrpke = 0x00000000,
	.grp_addds = 0x00000030,
	.grp_ctlds = 0x00000030,
	.grp_ddrmode = 0x00020000,
	.grp_b0ds = 0x00000030,
	.grp_b1ds = 0x00000030,
	.grp_b2ds = 0x00000030,
	.grp_b3ds = 0x00000030,
	.grp_b4ds = 0x00000030,
	.grp_b5ds = 0x00000030,
	.grp_b6ds = 0x00000030,
	.grp_b7ds = 0x00000030,
};

/* 4x128Mx16.cfg */
static const struct mx6_mmdc_calibration mx6_4x256mx16_mmdc_calib = {
	.p0_mpwldectrl0 = 0x002D0028,
	.p0_mpwldectrl1 = 0x0032002D,
	.p1_mpwldectrl0 = 0x00210036,
	.p1_mpwldectrl1 = 0x0019002E,
	.p0_mpdgctrl0 = 0x4349035C,
	.p0_mpdgctrl1 = 0x0348033D,
	.p1_mpdgctrl0 = 0x43550362,
	.p1_mpdgctrl1 = 0x03520316,
	.p0_mprddlctl = 0x41393940,
	.p1_mprddlctl = 0x3F3A3C47,
	.p0_mpwrdlctl = 0x413A423A,
	.p1_mpwrdlctl = 0x4042483E,
};

/* MT41K128M16JT-125 (2Gb density) */
static const struct mx6_ddr3_cfg mt41k128m16jt_125 = {
	.mem_speed = 1600,
	.density = 2,
	.width = 16,
	.banks = 8,
	.rowaddr = 14,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};

iomux_v3_cfg_t const uart_console_pads[] = {
	/* UART5 */
	MX6_PAD_CSI0_DAT14__UART5_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT15__UART5_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
};

void set_iomux_uart_spl(void)
{
	SETUP_IOMUX_PADS(uart_console_pads);
}

#ifdef CONFIG_MXC_SPI
iomux_v3_cfg_t const ecspi_pads[] = {
	MX6_PAD_EIM_EB2__GPIO2_IO30 | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_EIM_D18__ECSPI1_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_EIM_D17__ECSPI1_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_EIM_D16__ECSPI1_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL),
};

int board_spi_cs_gpio(unsigned int bus, unsigned int cs)
{
	if (bus != 0 || cs != 0)
		return -EINVAL;

	return IMX_GPIO_NR(2, 30);
}

void snappermx6_set_iomux_ecspi_spl(void)
{
	SETUP_IOMUX_PADS(ecspi_pads);
}

#else
void snappermx6_set_iomux_ecspi_spl(void) {}
#endif

static void ccgr_init(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	writel(0x00C03F3F, &ccm->CCGR0);
	writel(0x0030FC3F, &ccm->CCGR1);
	writel(0x0FFFCFC0, &ccm->CCGR2);
	writel(0x3FF00000, &ccm->CCGR3);
	writel(0x00FFF300, &ccm->CCGR4);
	writel(0x0F0000C3, &ccm->CCGR5);
	writel(0x000003FF, &ccm->CCGR6);
}

#ifdef CONFIG_MX6_DDRCAL
static void spl_dram_print_cal(struct mx6_ddr_sysinfo const *sysinfo)
{
	struct mx6_mmdc_calibration calibration = {0};

	mmdc_read_calibration(sysinfo, &calibration);

	debug(".p0_mpdgctrl0\t= 0x%08X\n", calibration.p0_mpdgctrl0);
	debug(".p0_mpdgctrl1\t= 0x%08X\n", calibration.p0_mpdgctrl1);
	debug(".p0_mprddlctl\t= 0x%08X\n", calibration.p0_mprddlctl);
	debug(".p0_mpwrdlctl\t= 0x%08X\n", calibration.p0_mpwrdlctl);
	debug(".p0_mpwldectrl0\t= 0x%08X\n", calibration.p0_mpwldectrl0);
	debug(".p0_mpwldectrl1\t= 0x%08X\n", calibration.p0_mpwldectrl1);
	debug(".p1_mpdgctrl0\t= 0x%08X\n", calibration.p1_mpdgctrl0);
	debug(".p1_mpdgctrl1\t= 0x%08X\n", calibration.p1_mpdgctrl1);
	debug(".p1_mprddlctl\t= 0x%08X\n", calibration.p1_mprddlctl);
	debug(".p1_mpwrdlctl\t= 0x%08X\n", calibration.p1_mpwrdlctl);
	debug(".p1_mpwldectrl0\t= 0x%08X\n", calibration.p1_mpwldectrl0);
	debug(".p1_mpwldectrl1\t= 0x%08X\n", calibration.p1_mpwldectrl1);
}

static void spl_dram_perform_cal(struct mx6_ddr_sysinfo const *sysinfo)
{
	int ret;

	/* Perform DDR DRAM calibration */
	udelay(100);
	ret = mmdc_do_write_level_calibration(sysinfo);
	if (ret) {
		printf("DDR: Write level calibration error [%d]\n", ret);
		return;
	}

	ret = mmdc_do_dqs_calibration(sysinfo);
	if (ret) {
		printf("DDR: DQS calibration error [%d]\n", ret);
		return;
	}

	spl_dram_print_cal(sysinfo);
}
#endif /* CONFIG_MX6_DDRCAL */

static void spl_dram_init(void)
{
	struct mx6_ddr_sysinfo sysinfo = {
		/* width of data bus:0=16,1=32,2=64 */
		.dsize = 2,
		/* config for full 4GB range so that get_mem_size() works */
		.cs_density = 32, /* 32Gb per CS */
		/* single chip select */
		.ncs = 1,
		.cs1_mirror = 0,
		.rtt_wr = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Wr = RZQ/4 */
		.rtt_nom = 2 /*DDR3_RTT_120_OHM*/,	/* RTT_Nom = RZQ/2 */
		.walat = 1,	/* Write additional latency */
		.ralat = 5,	/* Read additional latency */
		.mif3_mode = 3,	/* Command prediction working mode */
		.bi_on = 1,	/* Bank interleaving enabled */
		.sde_to_rst = 0x10,	/* 14 cycles, 200us (JEDEC default) */
		.rst_to_cke = 0x23,	/* 33 cycles, 500us (JEDEC default) */
		.pd_fast_exit = 1, /* enable precharge power-down fast exit */
		.ddr_type = DDR_TYPE_DDR3,
		.refsel = 1,	/* Refresh cycles at 32KHz */
		.refr = 7,	/* 8 refresh commands per refresh cycle */
	};

	mx6dq_dram_iocfg(64, &mx6_ddr_ioregs, &mx6_grp_ioregs);
	mx6_dram_cfg(&sysinfo, &mx6_4x256mx16_mmdc_calib, &mt41k128m16jt_125);

#ifdef CONFIG_MX6_DDRCAL
	spl_dram_perform_cal(&sysinfo);
#endif
}

static void init_ecspi(void)
{
	snappermx6_set_iomux_ecspi_spl();
	enable_spi_clk(1, 1);
}

void board_debug_uart_init(void)
{
	ccgr_init();
	set_iomux_uart_spl();

	/* Turn on the RS232 transceiver */
	gpio_direction_output(IMX_GPIO_NR(1, 9), 1);
}

void board_init_f(ulong dummy)
{
	int ret;

	board_debug_uart_init();

	arch_cpu_init();

	gpr_init();

	/* setup GP timer */
	timer_init();

	ret = spl_early_init();
	if (ret)
		panic("SPL early init failed");

	/* UART clocks enabled and gd valid - init serial console */
	preloader_console_init();

	if (IS_ENABLED(CONFIG_SPL_SPI_SUPPORT))
		init_ecspi();

	/* DDR initialization */
	spl_dram_init();

	/* Clear the BSS. */
	memset(__bss_start, '\0', __bss_end - __bss_start);

	/* Initialize and reset WDT in SPL */
// 	hw_watchdog_init();
// 	WATCHDOG_RESET();
#ifdef CONFIG_SPL_SPI_BOOT
	fsl_spi_spl_load_image(CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE,
			       (uchar *)SPL_ENV_ADDR);
	fsl_spi_boot();
#endif

	/* load/boot image from boot device */
	board_init_r(NULL, 0);
}

void board_boot_order(u32 *spl_boot_list)
{
	/* Default boot sequence SPI -> MMC */
	spl_boot_list[0] = spl_boot_device();
	spl_boot_list[1] = BOOT_DEVICE_MMC1;
	spl_boot_list[2] = BOOT_DEVICE_UART;
	spl_boot_list[3] = BOOT_DEVICE_NONE;

	/* Example of reading env */
#ifdef CONFIG_SPL_ENV_SUPPORT
	/* 'fastboot' */
	const char *s;

	if (env_init() || env_load())
		return;

	s = env_get("BOOT_FROM");
	if (s && !bootcount_error() && strcmp(s, "ACTIVE") == 0) {
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		spl_boot_list[1] = spl_boot_device();
	}
#endif
}

void reset_cpu(ulong addr) {}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	return 0;
}
#endif
