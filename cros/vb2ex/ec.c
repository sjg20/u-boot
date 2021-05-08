// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of EC callbacks
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY UCLASS_CROS_EC

#include <common.h>
#include <dm.h>
#include <cros_ec.h>
#include <log.h>
#include <malloc.h>
#include <vb2_api.h>
#include <cros/cros_common.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>
#include <cros/vboot_ec.h>
#include <cros/vboot_flag.h>
#include <cros/aux_fw.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

int vb2ex_ec_trusted(void)
{
	int gpio_ec_in_rw;
	int okay;

	log_debug("start\n");

	/* If we don't have a valid GPIO to read, we can't trust it */
	gpio_ec_in_rw = vboot_flag_read_walk(VBOOT_FLAG_EC_IN_RW);
	if (gpio_ec_in_rw < 0) {
		log_debug("can't find GPIO to read, returning 0\n");
		return 0;
	}

	/* We only trust it if it's NOT in its RW firmware */
	okay = !gpio_ec_in_rw;

	log_debug("value=%d, returning %d\n", gpio_ec_in_rw, okay);

	return okay;
}

/**
 * ec_get() - Get the EC based on the index
 *
 * @devidx: EC index (0 for first, 1 for second, etc.)
 * @devp: Returns the EC found on success
 * @return 0 if OK, -ve on error
 */
static int ec_get(int devidx, struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_CROS_VBOOT_EC, devidx, &dev);
	if (ret) {
		log_err("Get EC %d: err=%d\n", devidx, ret);
		return VB2_ERROR_UNKNOWN;
	}
	log_debug("EC devidx=%d,name=%s\n", devidx, dev->name);
	*devp = dev;

	return 0;
}

vb2_error_t vb2ex_ec_running_rw(int *in_rw)
{
	struct udevice *dev;
	int ret;

	log_debug("start\n");
	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);

	ret = vboot_ec_running_rw(dev, in_rw);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_jump_to_rw(void)
{
	struct udevice *dev;
	int ret;

	log_debug("start\n");
	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);

	ret = vboot_ec_jump_to_rw(dev);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_disable_jump(void)
{
	struct udevice *dev;
	int ret;

	log_debug("start\n");
	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);

	ret = vboot_ec_disable_jump(dev);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_hash_image(enum vb2_firmware_selection select,
				const u8 **hashp, int *hash_sizep)
{
	struct udevice *dev;
	int ret;

	log_debug("start\n");
	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);

	ret = vboot_ec_hash_image(dev, select, hashp, hash_sizep);
	log_debug("ret=%d, hash ptr=%p, hash_size=%x\n", ret, *hashp,
		  *hash_sizep);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

/**
 * get_firmware_entry() - Find the firmware entry for an EC
 *
 * @vboot: vboot info
 * @devidx: EC index (0 for first, 1 for second, etc.)
 * @select: Firmware to select
 * @return pointer to the entry containing the EC to use, NULL on error
 */
static struct fmap_entry *get_firmware_entry(struct vboot_info *vboot,
					     int devidx,
					     enum vb2_firmware_selection select)
{
	struct fmap_section *fw;
	struct fmap_ec *ec;
	struct fmap_entry *entry;

	fw = vboot_is_slot_a(vboot) ? &vboot->fmap.readwrite_a :
		&vboot->fmap.readwrite_b;
	if (devidx < 0 || devidx >= EC_COUNT) {
		log_err("entry not found, slot=%s, devidx=%d, select=%d",
			vboot_slot_name(vboot), devidx, select);
		return NULL;
	}
	ec = &fw->ec[devidx];
	entry = select == VB_SELECT_FIRMWARE_READONLY ? &ec->ro : &ec->rw;
	log_debug("Selected devidx=%d, select=%s\n", devidx,
		  select == VB_SELECT_FIRMWARE_READONLY ? "ro" : "rw");
	log_debug("entry->hash=%p, hash_size=%x\n", entry->hash,
		  entry->hash_size);

	return entry;
}

vb2_error_t ec_get_image(enum vb2_firmware_selection select, struct abuf **bufp)
{
	struct vboot_info *vboot = vboot_get();
	struct fmap_entry *entry;
	struct abuf *buf;
	int ret;

	log_debug("start\n");
	entry = get_firmware_entry(vboot, 0, select);
	if (!entry)
		return VB2_ERROR_UNKNOWN;

	/* Reuse the same image to avoid leaking memory */
	buf = &vboot->expected_ec_image;
	ret = fwstore_load_image(vboot->fwstore, entry, buf);
	if (ret) {
		log_err("Cannot locate image: err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}
	*bufp = buf;

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_get_expected_image_hash(enum vb2_firmware_selection select,
					     const u8 **hash, int *hash_size)
{
	struct vboot_info *vboot = vboot_get();
	struct fmap_entry *entry;
	int i;

	log_debug("start\n");
	entry = get_firmware_entry(vboot, 0, select);
	if (!entry) {
		log_err("Cannot get firmware entry:select=%d\b", select);
		return VB2_ERROR_UNKNOWN;
	}
	*hash = entry->hash;
	*hash_size = entry->hash_size;
	log_debug("Expected: ");
	for (i = 0; i < entry->hash_size; i++)
		log_debug("%02x", entry->hash[i]);
	log_debug("\n");

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_update_image(enum vb2_firmware_selection select)
{
	struct abuf *buf;
	struct udevice *dev;
	int ret;

	log_debug("start\n");
	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);

	ret = ec_get_image(select, &buf);
	if (ret)
		return log_msg_ret("image", ret);

	ret = vboot_ec_update_image(dev, select, buf);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		switch (ret) {
		case -EINVAL:
			return VB2_ERROR_INVALID_PARAMETER;
		case -EPERM:
			return VB2_REQUEST_REBOOT_EC_TO_RO;
		case -EIO:
		default:
			return VB2_ERROR_UNKNOWN;
		}
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_protect(enum vb2_firmware_selection select)
{
	struct udevice *dev;
	int ret;

	log_debug("start\n");
	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);

	ret = vboot_ec_protect(dev, select);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

/* Wait 3 seconds after software sync for EC to clear the limit power flag */
#define LIMIT_POWER_WAIT_TIMEOUT 3000
/* Check the limit power flag every 50 ms while waiting */
#define LIMIT_POWER_POLL_SLEEP 50

vb2_error_t vb2ex_ec_vboot_done(struct vb2_context *ctx)
{
	struct vboot_info *vboot = ctx_to_vboot(ctx);
	struct udevice *dev, *cros_ec;
	int limit_power;
	int ret;

	ret = ec_get(0, &dev);
	if (ret)
		return log_msg_ret("ec", ret);
	cros_ec = dev_get_parent(dev);

	log_debug("start\n");
	/* Ensure we have enough power to continue booting */
	while (1) {
		bool message_printed = false;
		int limit_power_wait_time = 0;
		int ret;

		ret = cros_ec_read_limit_power(cros_ec, &limit_power);
		if (ret == -ENOSYS) {
			limit_power = 0;
		} else if (ret) {
			log_warning("Failed to check EC limit power flag\n");
			return VB2_ERROR_UNKNOWN;
		}

		/*
		 * Do not wait for the limit power flag to be cleared in
		 * recovery mode since we didn't just sysjump.
		 */
		if (!limit_power || vboot_is_recovery(vboot) ||
		    limit_power_wait_time > LIMIT_POWER_WAIT_TIMEOUT)
			break;

		if (!message_printed) {
			log_info("Waiting for EC to clear limit power flag\n");
			message_printed = 1;
		}

		mdelay(LIMIT_POWER_POLL_SLEEP);
		limit_power_wait_time += LIMIT_POWER_POLL_SLEEP;
	}

	if (limit_power) {
		log_info("EC requests limited power usage. Request shutdown\n");
		return VB2_REQUEST_SHUTDOWN;
	}

	bootstage_mark(BOOTSTAMP_VBOOT_EC_DONE);

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_battery_cutoff(void)
{
	struct udevice *dev = board_get_cros_ec_dev();
	int ret;

	log_debug("start\n");
	if (!dev) {
		log_warning("No EC\n");
		return VB2_ERROR_UNKNOWN;
	}
	ret = cros_ec_battery_cutoff(dev, EC_BATTERY_CUTOFF_FLAG_AT_SHUTDOWN);
	if (ret) {
		log_err("Failed, err=%d\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}
