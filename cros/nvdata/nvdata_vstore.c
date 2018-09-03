// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_NVDATA

#include <common.h>
#include <dm.h>
#include <cros_ec.h>
#include <log.h>
#include <cros/nvdata.h>

static int cros_ec_nvdata_read(struct udevice *dev, enum cros_nvdata_type type,
			       u8 *data, int size)
{
	struct udevice *cros_ec = dev_get_parent(dev);
	int num_slots;
	u32 locked;
	int ret;

	if (index != CROS_NV_VSTORE) {
		log_debug("Only CROS_NV_VSTORE supported (not %x)\n", index);
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
		return -ENOSPC;
	}

	if (!(locked & VBOOT_HASH_VSLOT_MASK)) {
		log_err("Vstore slot not locked after write\n");
		return -EPERM;
	}

	return 0;
}

static int cros_ec_nvdata_write(struct udevice *dev, enum cros_nvdata_type type,
				const u8 *data, int size)
{
	struct udevice *cros_ec = dev_get_parent(dev);

	if (index != CROS_NV_DATA) {
		log_debug("Only CROS_NV_DATA supported (not %x)\n", index);
		return -ENOSYS;
	}

	return cros_ec_write_nvdata(cros_ec, data, size);
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
};
