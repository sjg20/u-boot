// SPDX-License-Identifier: GPL-2.0+
/*
 * Chromium OS vboot EC uclass, used for vboot operations implemented by an EC
 *
 * Copyright 2018 Google LLC
 */

#define LOG_DEBUG
#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <dm.h>
#include <log.h>
#include <cros_ec.h>
#include <cros/vboot.h>
#include <cros/vboot_ec.h>

static int cros_ec_vboot_running_rw(struct udevice *dev, int *in_rw)
{
	struct udevice *ec_dev = dev_get_parent(dev);
	enum ec_current_image image;
	int ret;

	ret = cros_ec_read_current_image(ec_dev, &image);
	if (ret < 0)
		return ret;

	switch (image) {
	case EC_IMAGE_RO:
		*in_rw = 0;
		break;
	case EC_IMAGE_RW:
		*in_rw = 1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cros_ec_vboot_jump_to_rw(struct udevice *dev)
{
	struct udevice *ec_dev = dev_get_parent(dev);
	int ret;

	ret = cros_ec_reboot(ec_dev, EC_REBOOT_JUMP_RW, 0);
	if (ret < 0)
		return ret;

	return 0;
}

static int cros_ec_vboot_disable_jump(struct udevice *dev)
{
	struct udevice *ec_dev = dev_get_parent(dev);
	int ret;

	ret = cros_ec_reboot(ec_dev, EC_REBOOT_DISABLE_JUMP, 0);
	if (ret < 0)
		return ret;

	return 0;
}

static u32 get_vboot_hash_offset(enum VbSelectFirmware_t select)
{
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		return EC_VBOOT_HASH_OFFSET_RO;
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		return EC_VBOOT_HASH_OFFSET_UPDATE;
	default:
		return EC_VBOOT_HASH_OFFSET_ACTIVE;
	}
}

static int cros_ec_vboot_hash_image(struct udevice *dev,
				    enum VbSelectFirmware_t select,
				    u8 *hash, int *hash_sizep)
{
	struct udevice *ec_dev = dev_get_parent(dev);
	static struct ec_response_vboot_hash resp;
	u32 hash_offset;
	int ret;
	uint i;

	hash_offset = get_vboot_hash_offset(select);

	ret = cros_ec_read_hash(ec_dev, hash_offset, &resp);
	if (ret)
		return log_msg_ret("read", ret);
	if (resp.digest_size > *hash_sizep)
		return log_msg_ret("size", -E2BIG);
	log_info("hash status=%x, hash_type=%x, digest_size=%x, offset=%x, size=%x\n",
		 resp.status, resp.hash_type, resp.digest_size, resp.offset,
		 resp.size);
	memcpy(hash, resp.hash_digest, resp.digest_size);
	for (i = 0; i < resp.digest_size; i++)
		printf("%02x", hash[i]);
	printf("\n");

	*hash_sizep = resp.digest_size;

	return 0;
}

static int vboot_set_region_protection(struct udevice *ec_dev,
				       enum VbSelectFirmware_t select,
				       int enable)
{
	struct ec_response_flash_protect resp;
	u32 protected_region = EC_FLASH_PROTECT_ALL_NOW;
	u32 mask = EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_ALL_AT_BOOT;
	int ret;

	if (select == VB_SELECT_FIRMWARE_READONLY)
		protected_region = EC_FLASH_PROTECT_RO_NOW;

	/* Update protection */
	log_debug("ec=%s, mask=%x, enable=%d\n", ec_dev->name, mask, enable);
	ret = cros_ec_flash_protect(ec_dev, mask, enable ? mask : 0, &resp);
	if (ret < 0) {
		log_err("Failed to update EC flash protection\n");
		return ret;
	}

	if (!enable) {
		/* If protection is still enabled, need reboot */
		if (resp.flags & protected_region)
			return -EPERM;

		return 0;
	}

	/*
	 * If write protect and ro-at-boot aren't both asserted, don't expect
	 * protection enabled.
	 */
	if (~resp.flags & (EC_FLASH_PROTECT_GPIO_ASSERTED |
			   EC_FLASH_PROTECT_RO_AT_BOOT))
		return 0;

	/* If flash is protected now, success */
	if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
		return 0;

	/* If RW will be protected at boot but not now, need a reboot */
	if (resp.flags & EC_FLASH_PROTECT_ALL_AT_BOOT)
		return -EPERM;

	/* Otherwise, it's an error */
	return -EIO;
}

static enum ec_flash_region vboot_to_ec_region(enum VbSelectFirmware_t select)
{
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		return EC_FLASH_REGION_WP_RO;
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		return EC_FLASH_REGION_UPDATE;
	default:
		return EC_FLASH_REGION_ACTIVE;
	}
}

static int cros_ec_vboot_update_image(struct udevice *dev,
				      enum VbSelectFirmware_t select,
				      const u8 *image, int image_size)
{
	struct udevice *ec_dev = dev_get_parent(dev);
	u32 region_offset, region_size;
	enum ec_flash_region region;
	int ret;

	region = vboot_to_ec_region(select);
	ret = vboot_set_region_protection(ec_dev, select, 0);
	if (ret)
		return log_msg_ret("prot", ret);

	ret = cros_ec_flash_offset(ec_dev, region, &region_offset,
				   &region_size);
	if (ret)
		return ret;
	log_info("Updating region %d, offset=%x, size=%x\n", region,
		 region_offset, region_size);
	if (image_size > region_size)
		return log_msg_ret("size", -EINVAL);

	/*
	 * Erase the entire region, so that the EC doesn't see any garbage
	 * past the new image if it's smaller than the current image.
	 *
	 * TODO: could optimise this to erase just the current image, since
	 * presumably everything past that is 0xff's.  But would still need to
	 * round up to the nearest multiple of erase size.
	 */
	ret = cros_ec_flash_erase(ec_dev, region_offset, region_size);
	if (ret)
		return log_msg_ret("erase", ret);

	/* Write the image */
	ret = cros_ec_flash_write(ec_dev, image, region_offset, image_size);
	if (ret)
		return log_msg_ret("write", ret);

	/* Verify the image */
	ret = cros_ec_efs_verify(ec_dev, region);
	if (ret)
		return log_msg_ret("verify", ret);
	log_info("EC image updated\n");

	return 0;
}

static int cros_ec_vboot_protect(struct udevice *dev,
				 enum VbSelectFirmware_t select)
{
	struct udevice *ec_dev = dev_get_parent(dev);

	return vboot_set_region_protection(ec_dev, select, 1);
}

static int cros_ec_vboot_entering_mode(struct udevice *dev,
				       enum VbEcBootMode_t mode)
{
	struct udevice *ec_dev = dev_get_parent(dev);
	int ret;

	ret = cros_ec_entering_mode(ec_dev, mode);
	if (ret)
		return ret;

	return 0;
}

int cros_ec_vboot_reboot_to_ro(struct udevice *dev)
{
	return 0;
}

static const struct vboot_ec_ops cros_ec_vboot_ops = {
	.running_rw	= cros_ec_vboot_running_rw,
	.jump_to_rw	= cros_ec_vboot_jump_to_rw,
	.running_rw	= cros_ec_vboot_running_rw,
	.disable_jump	= cros_ec_vboot_disable_jump,
	.hash_image	= cros_ec_vboot_hash_image,
	.update_image	= cros_ec_vboot_update_image,
	.protect	= cros_ec_vboot_protect,
	.entering_mode	= cros_ec_vboot_entering_mode,
	.reboot_to_ro	= cros_ec_vboot_reboot_to_ro,
};

static const struct udevice_id cros_ec_vboot_ids[] = {
	{ .compatible = "google,cros-ec-vboot" },
	{ }
};

U_BOOT_DRIVER(google_cros_ec_vboot) = {
	.name		= "google_cros_ec_vboot",
	.id		= UCLASS_CROS_VBOOT_EC,
	.of_match	= cros_ec_vboot_ids,
	.ops		= &cros_ec_vboot_ops,
};
