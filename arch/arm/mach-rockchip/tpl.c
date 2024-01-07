// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <bootstage.h>
#include <debug_uart.h>
#include <display_options.h>
#include <dm.h>
#include <hang.h>
#include <init.h>
#include <log.h>
#include <mapmem.h>
#include <ram.h>
#include <spl.h>
#include <version.h>
#include <asm/io.h>
#include <asm/arch-rockchip/bootrom.h>
#include <asm/arch-rockchip/timer.h>
#include <linux/bitops.h>

#include <syscon.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/grf_rk3399.h>
#include <asm/arch-rockchip/hardware.h>

#if CONFIG_IS_ENABLED(BANNER_PRINT)
#include <timestamp.h>
#endif

void board_init_f(ulong dummy)
{
	struct rk3399_pmusgrf_regs *sgrf;
	struct udevice *dev;
	int ret;

#if defined(CONFIG_DEBUG_UART) && defined(CONFIG_TPL_SERIAL)
	/*
	 * Debug UART can be used from here if required:
	 *
	 * debug_uart_init();
	 * printch('a');
	 * printhex8(0x1234);
	 * printascii("string");
	 */
	debug_uart_init();
	printch('a');
#ifdef CONFIG_TPL_BANNER_PRINT
	printascii("\nU-Boot TPL " PLAIN_VERSION " (" U_BOOT_DATE " - " \
				U_BOOT_TIME ")\n");
#endif
#endif
	/* Init secure timer */
	rockchip_stimer_init();

	sram_check("before spl_early_init()");
	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
		hang();
	}
	sram_check("after spl_early_init()");
	arch_cpu_init();
	sram_check("after arch_cpu_init()");

	sgrf = syscon_get_first_range(ROCKCHIP_SYSCON_PMUSGRF);
	printf("sgrf=%p, sgrf->slv_secure_con4=%x\n", sgrf,
	       readl(&sgrf->slv_secure_con4));

	/* Init ARM arch timer */
	if (IS_ENABLED(CONFIG_SYS_ARCH_TIMER))
		timer_init();

	if (CONFIG_IS_ENABLED(RAM)) {
		ret = uclass_get_device(UCLASS_RAM, 0, &dev);
		if (ret) {
			printf("DRAM init failed: %d\n", ret);
			return;
		}
	}

	printf("booting\n");
	sram_check("end of board_init_f()");
}

int board_return_to_bootrom(struct spl_image_info *spl_image,
			    struct spl_boot_device *bootdev)
{
#ifdef CONFIG_BOOTSTAGE_STASH
	int ret;

	bootstage_mark_name(BOOTSTAGE_ID_END_TPL, "end tpl");
	ret = bootstage_stash((void *)CONFIG_BOOTSTAGE_STASH_ADDR,
			      CONFIG_BOOTSTAGE_STASH_SIZE);
	if (ret)
		debug("Failed to stash bootstage: err=%d\n", ret);
#endif
	back_to_bootrom(BROM_BOOT_NEXTSTAGE);

	return 0;
}

u32 spl_boot_device(void)
{
	return IS_ENABLED(CONFIG_VPL) ? BOOT_DEVICE_MMC1 : BOOT_DEVICE_BOOTROM;
}

__weak struct legacy_img_hdr *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return map_sysmem(CONFIG_VPL_TEXT_BASE + offset, size);
}
