// SPDX-License-Identifier: GPL-2.0+
/*
 * Chromium OS vboot EC uclass, used for vboot operations implemented by an EC
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <cros_ec.h>
#include <cros/vboot.h>
#include <cros/vboot_ec.h>

int vboot_ec_running_rw(struct udevice *dev, int *in_rwp)
{
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->running_rw)
		return -ENOSYS;

	return ops->running_rw(dev, in_rwp);
}

int vboot_ec_jump_to_rw(struct udevice *dev)
{
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->jump_to_rw)
		return -ENOSYS;

	return ops->jump_to_rw(dev);
}

int vboot_ec_disable_jump(struct udevice *dev)
{
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->disable_jump)
		return -ENOSYS;

	return ops->disable_jump(dev);
}

int vboot_ec_hash_image(struct udevice *dev, enum vb2_firmware_selection select,
			const u8 **hashp, int *hash_sizep)
{
	struct vboot_ec_uc_priv *priv = dev_get_uclass_priv(dev);
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);
	int hash_size;
	int ret;

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->hash_image)
		return -ENOSYS;

	hash_size = VBOOT_EC_MAX_HASH_SIZE;
	ret = ops->hash_image(dev, select, priv->hash_digest, &hash_size);
	if (ret)
		return log_msg_ret("hash", ret);
	*hashp = priv->hash_digest;
	log_debug("returning hash %p %p\n", priv, priv->hash_digest);
	*hash_sizep = hash_size;

	return 0;
}

int vboot_ec_update_image(struct udevice *dev, enum vb2_firmware_selection select,
			  const struct abuf *buf)
{
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->update_image)
		return -ENOSYS;

	return ops->update_image(dev, select, buf);
}

int vboot_ec_protect(struct udevice *dev, enum vb2_firmware_selection select)
{
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->protect)
		return -ENOSYS;

	return ops->protect(dev, select);
}

int vboot_ec_reboot_to_ro(struct udevice *dev)
{
	struct vboot_ec_ops *ops = vboot_ec_get_ops(dev);

	if (device_get_uclass_id(dev) != UCLASS_CROS_VBOOT_EC)
		return -EDOM;
	if (!ops->reboot_to_ro)
		return -ENOSYS;

	return ops->reboot_to_ro(dev);
}

UCLASS_DRIVER(cros_vboot_ec) = {
	.id		= UCLASS_CROS_VBOOT_EC,
	.name		= "cros-vboot-ec",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_auto	= sizeof(struct vboot_ec_uc_priv),
};
