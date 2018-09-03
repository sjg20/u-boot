// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of verified-boot flags for Chromium OS
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_VBOOT_FLAG

#include <common.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <cros/vboot_flag.h>
#include <dm/device-internal.h>

/* Names for all the flag we know about */
static const char *const flag_name[] = {
	[VBOOT_FLAG_WRITE_PROTECT] = "write-protect",
	[VBOOT_FLAG_DEVELOPER] = "developer",
	[VBOOT_FLAG_LID_OPEN] = "lid-open",
	[VBOOT_FLAG_POWER_BUTTON] = "power-button",
	[VBOOT_FLAG_EC_IN_RW] = "ec-in-rw",
	[VBOOT_FLAG_OPROM_LOADED] = "oprom-loaded",
	[VBOOT_FLAG_RECOVERY] = "recovery",
	[VBOOT_FLAG_WIPEOUT] = "wipeout",
};

const char *vboot_flag_name(enum vboot_flag_t flag)
{
	return flag_name[flag];
}

int vboot_flag_read(struct udevice *dev)
{
	struct vboot_flag_ops *ops = vboot_flag_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev);
}

int vboot_flag_read_walk_prev(enum vboot_flag_t flag, int *prevp,
			      struct udevice **devp)
{
	struct vboot_flag_state *priv;
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	if (devp)
		*devp = NULL;
	if (prevp)
		*prevp = -1;

	ret = uclass_get(UCLASS_CROS_VBOOT_FLAG, &uc);
	if (ret)
		return log_msg_ret("uc", ret);
	priv = uclass_get_priv(uc);

	uclass_foreach_dev(dev, uc) {
		struct vboot_flag_uc_priv *uc_priv;

		ret = device_probe(dev);
		if (ret) {
			log_warning("Device '%s' failed to probe (err=%d)\n",
				    dev->name,  ret);
			continue;
		}

		uc_priv = dev_get_uclass_priv(dev);
		if (uc_priv->flag != flag)
			continue;

		/* Skip this flag if it is only for the primary bootloader */
		if (!ll_boot_init() && uc_priv->primary_only)
			continue;

		ret = vboot_flag_read(dev);
		if (ret == -ENOENT) {
			continue;
		} else if (ret < 0) {
			log_warning("%s: Failed to read\n", dev->name);
			break;
		}

		if (prevp)
			*prevp = priv->value[flag];
		if (devp)
			*devp = dev;
		priv->value[flag] = ret;
		return ret;
	}

	/* No devices provided the flag */
	return -ENOENT;
}

int vboot_flag_read_walk(enum vboot_flag_t flag)
{
	return vboot_flag_read_walk_prev(flag, NULL, NULL);
}

enum vboot_flag_t vboot_flag_find(const char *name)
{
	int i;

	for (i = 0; i < VBOOT_FLAG_COUNT; i++) {
		if (!strcmp(name, flag_name[i]))
			break;
	}
	if (i == VBOOT_FLAG_COUNT) {
		log_warning("Unrecognised flag name '%s'\n", name);
		return -EINVAL;
	}

	return (enum vboot_flag_t)i;
}

static int vboot_flag_pre_probe(struct udevice *dev)
{
	struct vboot_flag_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	const char *name;
	int ret;

	name = dev_read_string(dev, "google,name");
	if (!name)
		name = dev->name;
	ret = vboot_flag_find(name);
	if (ret < 0)
		return ret;

	uc_priv->flag = ret;
	uc_priv->primary_only = dev_read_bool(dev, "primary-only");

	return 0;
}

static int vboot_flag_init(struct uclass *uc)
{
	struct vboot_flag_state *uc_priv = uclass_get_priv(uc);
	int i;

	for (i = 0; i < VBOOT_FLAG_COUNT; i++)
		uc_priv->value[i] = -1;

	return 0;
}

UCLASS_DRIVER(vboot_flag) = {
	.id		= UCLASS_CROS_VBOOT_FLAG,
	.name		= "vboot_flag",
	.init		= vboot_flag_init,
	.pre_probe	= vboot_flag_pre_probe,
	.priv_auto	= sizeof(struct vboot_flag_state),
	.per_device_auto = sizeof(struct vboot_flag_uc_priv),
};
