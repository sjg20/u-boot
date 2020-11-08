// SPDX-License-Identifier: GPL-2.0+
/*
 * GPIO flag: read from the EC to determine a flag value
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_VBOOT_FLAG

#include <common.h>
#include <cros_ec.h>
#include <dm.h>
#include <log.h>
#include <cros/vboot_flag.h>

static int flag_cros_ec_read(struct udevice *dev)
{
	struct vboot_flag_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct udevice *cros_ec = dev_get_parent(dev);
	int ret;

	switch (uc_priv->flag) {
	case VBOOT_FLAG_LID_OPEN:
		ret = cros_ec_get_switches(cros_ec);

		if (ret < 0)
			return log_msg_ret("lid", ret);
		return !!(ret & EC_SWITCH_LID_OPEN);
	case VBOOT_FLAG_RECOVERY: {
		u32 events;

		ret = cros_ec_get_host_events(cros_ec, &events);
		if (ret)
			return log_msg_ret("rec", ret);
		return !!(events &
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_RECOVERY));
	}
	default:
		return -EINVAL;
	}

	return 0;
}

static int flag_cros_ec_probe(struct udevice *dev)
{
	struct vboot_flag_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct udevice *cros_ec = dev_get_parent(dev);

	if (device_get_uclass_id(cros_ec) != UCLASS_CROS_EC)
		return log_msg_ret("uc", -EPROTOTYPE);
	if (uc_priv->flag != VBOOT_FLAG_LID_OPEN &&
	    uc_priv->flag != VBOOT_FLAG_RECOVERY)
		return log_msg_ret("uc", -ENOTSUPP);

	return 0;
}

static const struct vboot_flag_ops flag_cros_ec_ops = {
	.read	= flag_cros_ec_read,
};

static const struct udevice_id flag_cros_ec_ids[] = {
	{ .compatible = "google,cros-ec-flag" },
	{ }
};

U_BOOT_DRIVER(flag_cros_ec_drv) = {
	.name		= "flag_cros_ec",
	.id		= UCLASS_CROS_VBOOT_FLAG,
	.of_match	= flag_cros_ec_ids,
	.probe		= flag_cros_ec_probe,
	.ops		= &flag_cros_ec_ops,
};
