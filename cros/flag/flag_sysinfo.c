// SPDX-License-Identifier: GPL-2.0+
/*
 * A vboot flag controlled by coreboot sysinfo tables (x86 only)
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY UCLASS_CROS_VBOOT_FLAG

#include <common.h>
#include <asm/cb_sysinfo.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <cros/vboot_flag.h>

/* Coreboot names for all the flag we know about */
static const char *const cb_flag_name[] = {
	[VBOOT_FLAG_WRITE_PROTECT] = "write protect",
	[VBOOT_FLAG_LID_OPEN] = "lid",
	[VBOOT_FLAG_POWER_BUTTON] = "power",
	[VBOOT_FLAG_EC_IN_RW] = "EC in RW",
	[VBOOT_FLAG_OPROM_LOADED] = "oprom",
	[VBOOT_FLAG_RECOVERY] = "recovery",
	[VBOOT_FLAG_WIPEOUT] = "wipeout",
};

/**
 * Private data for this driver
 *
 * @port: GPIO port number (coreboot value)
 * @active_high: true if active high, false if active low (inverted)
 * @value: raw GPIO value as read by coreboot
 */
struct flag_sysinfo_priv {
	int port;
	bool active_high;
	int value;
};

static int flag_sysinfo_read(struct udevice *dev)
{
	struct flag_sysinfo_priv *priv = dev_get_priv(dev);

	return priv->value;
}

static int flag_sysinfo_probe(struct udevice *dev)
{
	struct vboot_flag_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct flag_sysinfo_priv *priv = dev_get_priv(dev);
	const struct sysinfo_t *sysinfo;
	const char *cb_name;
	int i;

	sysinfo = cb_get_sysinfo();

	cb_name = cb_flag_name[uc_priv->flag];
	if (!cb_name) {
		log_warning("No coreboot name for flag '%s'\n", dev->name);
		return -ENOENT;
	}

	for (i = 0; i < sysinfo->num_gpios; i++) {
		if (strncmp((char *)sysinfo->gpios[i].name, cb_name,
			    CB_GPIO_MAX_NAME_LENGTH))
			continue;

		/* Entry found */
		priv->port = sysinfo->gpios[i].port;
		priv->active_high = sysinfo->gpios[i].polarity;
		priv->value = sysinfo->gpios[i].value;
		if (!priv->active_high)
			priv->value = !priv->value;

		return 0;
	}
	log_warning("No coreboot flag '%s' in sysinfo\n", cb_name);

	return -ENOTSUPP;
}

static const struct vboot_flag_ops flag_sysinfo_ops = {
	.read	= flag_sysinfo_read,
};

static const struct udevice_id flag_sysinfo_ids[] = {
	{ .compatible = "google,sysinfo-flag" },
	{ }
};

U_BOOT_DRIVER(google_sysinfo_flag) = {
	.name		= "google_sysinfo_flag",
	.id		= UCLASS_CROS_VBOOT_FLAG,
	.of_match	= flag_sysinfo_ids,
	.probe		= flag_sysinfo_probe,
	.ops		= &flag_sysinfo_ops,
	.priv_auto	= sizeof(struct flag_sysinfo_priv),
};
