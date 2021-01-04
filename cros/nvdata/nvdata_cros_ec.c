// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY UCLASS_CROS_NVDATA

#include <common.h>
#include <dm.h>
#include <cros_ec.h>
#include <log.h>
#include <cros/nvdata.h>

#define VBOOT_HASH_VSLOT	0
#define VBOOT_HASH_VSLOT_MASK	(1 << (VBOOT_HASH_VSLOT))

struct nvdata_priv {
	u32 supported;
};

static int cros_ec_nvdata_read(struct udevice *dev, enum cros_nvdata_type type,
			       u8 *data, int size)
{
	struct udevice *cros_ec = dev_get_parent(dev);
	struct nvdata_priv *priv = dev_get_priv(dev);
	int ret;

	if (type != CROS_NV_DATA && type != CROS_NV_VSTORE) {
		log_debug("Type %x not supported\n", type);
		return -ENOSYS;
	}
	if (!(priv->supported & ((1 << type)))) {
		log_debug("Type %x not enabled (mask %x)\n", type,
			  priv->supported);
		return -ENOSYS;
	}
	switch (type) {
	case CROS_NV_DATA:
		ret = cros_ec_read_nvdata(cros_ec, data, size);
		break;
	case CROS_NV_VSTORE: {
		ret = cros_ec_vstore_read(cros_ec, VBOOT_HASH_VSLOT, data);
		if (ret)
			return log_msg_ret("write", ret);

		break;
	}
	default:
		log_debug("Type %x not supported\n", type);
		return -ENOSYS;
	}

	return 0;
}

static int cros_ec_nvdata_write(struct udevice *dev, enum cros_nvdata_type type,
				const u8 *data, int size)
{
	struct udevice *cros_ec = dev_get_parent(dev);
	struct nvdata_priv *priv = dev_get_priv(dev);
	int ret;

	if (type != CROS_NV_DATA && type != CROS_NV_VSTORE) {
		log_debug("Type %x not supported\n", type);
		return -ENOSYS;
	}
	if (!(priv->supported & ((1 << type)))) {
		log_debug("Type %x not enabled (mask %x)\n", type,
			  priv->supported);
		return -ENOSYS;
	}
	switch (type) {
	case CROS_NV_DATA:
		ret = cros_ec_write_nvdata(cros_ec, data, size);
		break;
	case CROS_NV_VSTORE: {
		u32 locked;
		int num_slots;

		if (cros_ec_vstore_supported(cros_ec) != true) {
			log_warning("Not supported\n");
			return -ENOSYS;
		}
		ret = cros_ec_vstore_write(cros_ec, VBOOT_HASH_VSLOT, data,
					   size);
		if (ret)
			return log_msg_ret("write", ret);

		/* Check the slot is now locked */
		ret = cros_ec_vstore_info(cros_ec, &locked);
		if (ret)
			return log_msg_ret("info", ret);

		num_slots = ret;
		if (VBOOT_HASH_VSLOT >= num_slots) {
			log_err("Not enough vstore slots (have %d, need %d)\n",
				num_slots, VBOOT_HASH_VSLOT + 1);
			return log_msg_ret("slots", -ENOSPC);
		}

		if (!(locked & VBOOT_HASH_VSLOT_MASK)) {
			log_err("Vstore slot not locked after write\n");
			return log_msg_ret("lock", -EPERM);
		}
		break;
	}
	default:
		log_debug("Type %x not supported\n", type);
		return -ENOSYS;
	}

	return 0;
}

static int cros_ec_ofdata_to_platdata(struct udevice *dev)
{
	struct nvdata_priv *priv = dev_get_priv(dev);

	if (dev_read_u32(dev, "nvdata,types", &priv->supported))
		return log_ret(-EINVAL);

	return 0;
}

static const struct cros_nvdata_ops cros_ec_nvdata_ops = {
	.read	= cros_ec_nvdata_read,
	.write	= cros_ec_nvdata_write,
};

static const struct udevice_id cros_ec_nvdata_ids[] = {
	{ .compatible = "google,cros-ec-nvdata" },
	{ }
};

U_BOOT_DRIVER(cros_ec_nvdata_drv) = {
	.name		= "cros-ec-nvdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= cros_ec_nvdata_ids,
	.ops		= &cros_ec_nvdata_ops,
	.ofdata_to_platdata	= cros_ec_ofdata_to_platdata,
	.priv_auto_alloc_size	= sizeof(struct nvdata_priv),
};
