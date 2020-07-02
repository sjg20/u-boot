// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>

#if !CONFIG_IS_ENABLED(TINY_SYSCON)
static const struct udevice_id rk3288_syscon_ids[] = {
	{ .compatible = "rockchip,rk3288-noc", .data = ROCKCHIP_SYSCON_NOC },
	{ .compatible = "rockchip,rk3288-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ .compatible = "rockchip,rk3288-sgrf", .data = ROCKCHIP_SYSCON_SGRF },
	{ .compatible = "rockchip,rk3288-pmu", .data = ROCKCHIP_SYSCON_PMU },
	{ }
};

U_BOOT_DRIVER(syscon_rk3288) = {
	.name = "rk3288_syscon",
	.id = UCLASS_SYSCON,
	.of_match = rk3288_syscon_ids,
};

U_BOOT_DRIVER_ALIAS(syscon_rk3288, rockchip_rk3288_noc)
U_BOOT_DRIVER_ALIAS(syscon_rk3288, rockchip_rk3288_pmu)
U_BOOT_DRIVER_ALIAS(syscon_rk3288, rockchip_rk3288_grf)
U_BOOT_DRIVER_ALIAS(syscon_rk3288, rockchip_rk3288_sgrf)

#else

U_BOOT_TINY_DRIVER(syscon_rk3288) = {
	.uclass_id	= UCLASS_SYSCON,
	.probe		= tiny_syscon_setup,
// 	.ops		= &rockchip_clk_tiny_ops,
	DM_TINY_PRIV(<asm/arch-rockchip/clock.h>, \
		     sizeof(struct syscon_uc_info))
};
#endif
