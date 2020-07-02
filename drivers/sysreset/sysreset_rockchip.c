// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 */

#define LOG_CATEGORY UCLASS_SYSRESET

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <sysreset.h>
#include <asm/io.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/cru_rk3328.h>
#include <asm/arch-rockchip/hardware.h>
#include <linux/err.h>

static int rockchip_sysreset_request_(struct sysreset_reg *priv,
				      enum sysreset_t type)
{
	unsigned long cru_base = (unsigned long)rockchip_get_cru();

	if (IS_ERR_VALUE(cru_base))
		return (int)cru_base;

	switch (type) {
	case SYSRESET_WARM:
		writel(0xeca8, cru_base + priv->glb_srst_snd_value);
		break;
	case SYSRESET_COLD:
		writel(0xfdb9, cru_base + priv->glb_srst_fst_value);
		break;
	default:
		return -EPROTONOSUPPORT;
	}

	return -EINPROGRESS;
}

#if !CONFIG_IS_ENABLED(TINY_SYSRESET)
int rockchip_sysreset_request(struct udevice *dev, enum sysreset_t type)
{
	struct sysreset_reg *priv = dev_get_priv(dev);

	return rockchip_sysreset_request_(priv, type);
}

static int rockchip_sysreset_probe(struct udevice *dev)
{
	return rockchip_cru_setup_sysreset(dev);
}

static struct sysreset_ops rockchip_sysreset_ops = {
	.request	= rockchip_sysreset_request,
};

static const struct udevice_id rockchip_sysreset_ids[] = {
	{ .compatible = "rockchip,sysreset" },
	{ }
};

U_BOOT_DRIVER(rockchip_sysreset) = {
	.name	= "rockchip_sysreset",
	.id	= UCLASS_SYSRESET,
	.of_match = rockchip_sysreset_ids,
	.ops	= &rockchip_sysreset_ops,
	.probe	= rockchip_sysreset_probe,
	.priv_auto_alloc_size	= sizeof(struct sysreset_reg),
};
#else
int rockchip_sysreset_tiny_request(struct tinydev *tdev, enum sysreset_t type)
{
	struct sysreset_reg *priv = tinydev_get_priv(tdev);

	return rockchip_sysreset_request_(priv, type);
}

static int rockchip_sysreset_tiny_probe(struct tinydev *tdev)
{
	return rockchip_cru_setup_tiny_sysreset(tdev);
}

static struct tiny_sysreset_ops rockchip_sysreset_tiny_ops = {
	.request	= rockchip_sysreset_tiny_request,
};

U_BOOT_TINY_DRIVER(rockchip_sysreset) = {
	.uclass_id	= UCLASS_SYSRESET,
	.probe		= rockchip_sysreset_tiny_probe,
	.ops		= &rockchip_sysreset_tiny_ops,
	DM_TINY_PRIV(<asm/arch-rockchip/clock.h>, sizeof(struct sysreset_reg))
};
#endif
