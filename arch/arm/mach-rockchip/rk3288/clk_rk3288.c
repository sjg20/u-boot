// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/cru.h>
#include <linux/err.h>

static int rockchip_sysreset_probe_(struct sysreset_reg *priv)
{
	priv->glb_srst_fst_value = offsetof(struct rockchip_cru,
					    cru_glb_srst_fst_value);
	priv->glb_srst_snd_value = offsetof(struct rockchip_cru,
					    cru_glb_srst_snd_value);

	return 0;
}

#if !CONFIG_IS_ENABLED(TINY_CLK)
int rockchip_get_clk(struct udevice **devp)
{
	return uclass_get_device_by_driver(UCLASS_CLK,
			DM_GET_DRIVER(rockchip_rk3288_cru), devp);
}

void *rockchip_get_cru(void)
{
	struct rk3288_clk_priv *priv;
	struct udevice *dev;
	int ret;

	ret = rockchip_get_clk(&dev);
	if (ret)
		return ERR_PTR(ret);

	priv = dev_get_priv(dev);

	return priv->cru;
}

int rockchip_cru_setup_sysreset(struct udevice *dev)
{
	struct sysreset_reg *priv = dev_get_priv(dev);

	return rockchip_sysreset_probe_(priv);
}

#else /* TINY_CLK */

struct tinydev *tiny_rockchip_get_clk(void)
{
	return tiny_dev_get(UCLASS_CLK, 0);
}

void *rockchip_get_cru(void)
{
	struct rk3288_clk_priv *priv;
	struct tinydev *tdev;

	tdev = tiny_rockchip_get_clk();
	if (!tdev)
		return NULL;
	priv = tdev->priv;

	return priv->cru;
}

int rockchip_cru_setup_tiny_sysreset(struct tinydev *tdev)
{
	struct sysreset_reg *priv = tinydev_get_priv(tdev);

	return rockchip_sysreset_probe_(priv);
}
#endif
