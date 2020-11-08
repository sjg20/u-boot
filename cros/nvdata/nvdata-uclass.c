// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_NVDATA

#include <common.h>
#include <dm.h>
#include <ec_commands.h>
#include <log.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>

int cros_nvdata_read(struct udevice *dev, enum cros_nvdata_type type, u8 *data,
		     int size)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, type, data, size);
}

int cros_nvdata_write(struct udevice *dev, enum cros_nvdata_type type,
		      const u8 *data, int size)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->write)
		return -ENOSYS;

	return ops->write(dev, type, data, size);
}

int cros_nvdata_setup(struct udevice *dev, enum cros_nvdata_type type,
		      uint attr, uint size,
		      const u8 *nv_policy, int nv_policy_size)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->setup)
		return -ENOSYS;

	return ops->setup(dev, type, attr, size, nv_policy, nv_policy_size);
}

int cros_nvdata_lock(struct udevice *dev, enum cros_nvdata_type type)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->lock)
		return -ENOSYS;

	return ops->lock(dev, type);
}

bool supports_type(struct udevice *dev, enum cros_nvdata_type type)
{
	struct nvdata_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	uint mask = 1 << type;

	return uc_priv->supported & mask;
}

int cros_nvdata_read_walk(enum cros_nvdata_type type, u8 *data, int size)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	uclass_foreach_dev_probe(UCLASS_CROS_NVDATA, dev) {
		if (supports_type(dev, type)) {
			ret = cros_nvdata_read(dev, type, data, size);
			if (!ret)
				break;
		}
	}
	if (ret)
		return ret;

	return 0;
}

int cros_nvdata_write_walk(enum cros_nvdata_type type, const u8 *data, int size)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	log_info("write type %d size %x\n", type, size);
	uclass_foreach_dev_probe(UCLASS_CROS_NVDATA, dev) {
		if (supports_type(dev, type)) {
			ret = cros_nvdata_write(dev, type, data, size);
			if (!ret)
				break;
		}
	}
	if (ret) {
		log_warning("Failed to write type %d\n", type);
		return ret;
	}

	return 0;
}

int cros_nvdata_setup_walk(enum cros_nvdata_type type, uint attr, uint size,
			   const u8 *nv_policy, uint nv_policy_size)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	uclass_foreach_dev_probe(UCLASS_CROS_NVDATA, dev) {
		if (supports_type(dev, type)) {
			ret = cros_nvdata_setup(dev, type, attr, size,
						nv_policy, nv_policy_size);
			if (!ret)
				break;
		}
	}
	if (ret)
		return ret;

	return 0;
}

int cros_nvdata_lock_walk(enum cros_nvdata_type type)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	uclass_foreach_dev_probe(UCLASS_CROS_NVDATA, dev) {
		if (supports_type(dev, type)) {
			ret = cros_nvdata_lock(dev, type);
			if (!ret)
				break;
		}
	}
	if (ret)
		return ret;

	return 0;
}

VbError_t VbExNvStorageRead(u8 *buf)
{
	int ret;

	ret = cros_nvdata_read_walk(CROS_NV_DATA, buf, EC_VBNV_BLOCK_SIZE);
	if (ret)
		return VBERROR_UNKNOWN;
#ifdef DEBUG
	print_buffer(0, buf, 1, EC_VBNV_BLOCK_SIZE, 0);
#endif

	return 0;
}

VbError_t VbExNvStorageWrite(const u8 *buf)
{
	int ret;

#ifdef DEBUG
	print_buffer(0, buf, 1, EC_VBNV_BLOCK_SIZE, 0);
#endif
	vboot_dump_nvdata(buf, EC_VBNV_BLOCK_SIZE);
	ret = cros_nvdata_write_walk(CROS_NV_DATA, buf, EC_VBNV_BLOCK_SIZE);
	if (ret)
		return VBERROR_UNKNOWN;

	return 0;
}

int cros_nvdata_ofdata_to_platdata(struct udevice *dev)
{
	struct nvdata_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	uint i;

	for (i = 0; i < 32; i++) {
		int ret;
		u32 val;

		ret = dev_read_u32_index(dev, "nvdata,types", i, &val);
		if (ret == -EOVERFLOW)
			break;
		else if (ret) {
			log_err("Device '%s' is missing nvdata,types\n",
				dev->name);
			return log_msg_ret("array", ret);
		}

		uc_priv->supported |= 1 << val;
	}

	return 0;
}

UCLASS_DRIVER(cros_nvdata) = {
	.id		= UCLASS_CROS_NVDATA,
	.name		= "cros_nvdata",
	.per_device_auto_alloc_size	= sizeof(struct nvdata_uc_priv),
};
