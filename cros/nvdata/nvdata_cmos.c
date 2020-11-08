// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_NVDATA

#include <common.h>
#include <dm.h>
#include <log.h>
#include <rtc.h>
#include <cros/nvdata.h>

struct cmos_priv {
	u32 base_reg;
};

static int cmos_nvdata_read(struct udevice *dev, enum cros_nvdata_type type,
			    u8 *data, int size)
{
	struct cmos_priv *priv = dev_get_priv(dev);
	struct udevice *rtc = dev_get_parent(dev);
	int ret;

	if (type != CROS_NV_DATA) {
		log_debug("Only CROS_NV_DATA supported (not %d)\n", type);
		return -ENOSYS;
	}

	ret = dm_rtc_read(rtc, priv->base_reg, data, size);
	if (ret)
		return log_msg_ret("Read CMOS RAM", ret);

	return 0;
}

static int cmos_nvdata_write(struct udevice *dev, enum cros_nvdata_type type, const u8 *data,
			     int size)
{
	struct cmos_priv *priv = dev_get_priv(dev);
	struct udevice *rtc = dev_get_parent(dev);
	int ret;

	if (type != CROS_NV_DATA) {
		log_debug("Only CROS_NV_DATA supported (not %d)\n", type);
		return -ENOSYS;
	}

	ret = dm_rtc_write(rtc, priv->base_reg, data, size);
	if (ret)
		return log_msg_ret("Write CMOS RAM", ret);

	return 0;
}

static int cmos_nvdata_probe(struct udevice *dev)
{
	struct cmos_priv *priv = dev_get_priv(dev);
	int ret;

	ret = dev_read_u32(dev, "reg", &priv->base_reg);
	if (ret)
		return log_msg_ret("Missing 'reg' property", ret);

	/* Add 14 to skip past the RTC registers */
	priv->base_reg += 14;

	return 0;
}

static const struct cros_nvdata_ops cmos_nvdata_ops = {
	.read	= cmos_nvdata_read,
	.write	= cmos_nvdata_write,
};

static const struct udevice_id cmos_nvdata_ids[] = {
	{ .compatible = "google,cmos-nvdata" },
	{ }
};

U_BOOT_DRIVER(google_cmos_nvdata) = {
	.name		= "google_cmos_nvdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= cmos_nvdata_ids,
	.ops		= &cmos_nvdata_ops,
	.priv_auto_alloc_size	= sizeof(struct cmos_priv),
	.probe		= cmos_nvdata_probe,
	.ofdata_to_platdata	= cros_nvdata_ofdata_to_platdata,
};
