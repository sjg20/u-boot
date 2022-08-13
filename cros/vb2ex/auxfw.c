// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of callbacks for updating auxiliary firmware (auxfx)
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

static int locate_aux_fw(struct udevice *dev, struct fmap_entry *entry)
{
	struct ofnode_phandle_args args;
	int ret;

	ret = ofnode_parse_phandle_with_args(dev_ofnode(dev), "firmware", NULL,
					     0, 0, &args);
	if (ret)
		return log_msg_ret("Cannot find firmware", ret);
	ret = ofnode_read_fmap_entry(args.node, entry);
	if (ret)
		return log_msg_ret("Cannot read fmap entry", ret);

	return 0;
}

vb2_error_t vb2ex_auxfw_check(enum vb2_auxfw_update_severity *severityp)
{
	enum aux_fw_severity max, current;
	struct udevice *dev;
	struct fmap_entry entry;
	int ret;

	max = VB2_AUXFW_NO_UPDATE;
	uclass_foreach_dev_probe(UCLASS_CROS_AUX_FW, dev) {
		ret = locate_aux_fw(dev, &entry);
		if (ret)
			return ret;
		if (!entry.hash)
			return log_msg_ret("Entry has no hash", -EINVAL);
		ret = aux_fw_check_hash(dev, entry.hash, entry.hash_size,
					&current);
		if (ret)
			return log_msg_ret("Check hashf failed", ret);
		max = max(max, current);
	}
	switch (max) {
	case AUX_FW_NO_UPDATE:
		*severityp = VB2_AUXFW_NO_UPDATE;
		break;
	case AUX_FW_FAST_UPDATE:
		*severityp = AUX_FW_FAST_UPDATE;
		break;
	case AUX_FW_SLOW_UPDATE:
		*severityp = AUX_FW_SLOW_UPDATE;
		break;
	default:
		log_err("Invalid severity %d", max);
		return VB2_ERROR_UNKNOWN;
	}

	return 0;
}

/**
 * struct aux_fw_state - Keeps track of the system state
 *
 * @power_button_disabled: true if the power button had to be disabled and
 *	should be re-enabled after firmware update is completed
 * @lid_shutdown_disabled: true if the lid-shutdown had to be disabled and
 *	should be re-enabled after firmware update is completed
 * @reboot_required: true if one of the updates requires a reboot to complete
 */
struct aux_fw_state {
	bool power_button_disabled;
	bool lid_shutdown_disabled;
	bool reboot_required;
};

/**
 * do_aux_fw_update() - handle updating the firmware on a device
 *
 * @vboot: vboot info
 * @dev: Device to update (UCLASS_CROS_AUX_FW)
 * @state: State to update
 * @return 0 if update completed, -ve on error
 */
static int do_aux_fw_update(struct vboot_info *vboot, struct udevice *dev,
			    struct aux_fw_state *state)
{
	enum aux_fw_severity severity;
	struct fmap_entry entry;
	struct abuf buf;
	int ret;

	if (!state->power_button_disabled &&
	    vboot->disable_power_button_during_update) {
		cros_ec_config_powerbtn(vboot->cros_ec, 0);
		state->power_button_disabled = 1;
	}
	if (!state->lid_shutdown_disabled &&
	    vboot->disable_lid_shutdown_during_update &&
	    cros_ec_get_lid_shutdown_mask(vboot->cros_ec) > 0) {
		if (!cros_ec_set_lid_shutdown_mask(vboot->cros_ec, 0))
			state->lid_shutdown_disabled = 1;
	}
	/* Apply update */
	ret = locate_aux_fw(dev, &entry);
	if (ret)
		return ret;

	log_info("Update aux fw '%s'\n", dev->name);
	abuf_init(&buf);
	ret = fwstore_load_image(dev, &entry, &buf);
	if (ret)
		return log_msg_ret("load", ret);

	ret = aux_fw_update_image(dev, abuf_data(&buf), abuf_size(&buf));
	abuf_uninit(&buf);
	if (ret == ERESTARTSYS)
		state->reboot_required = 1;
	else if (ret)
		return ret;
	/* Re-check hash after update */
	ret = aux_fw_check_hash(dev, entry.hash, entry.hash_size, &severity);
	if (ret)
		return log_msg_ret("Check hash failed", ret);
	if (severity != AUX_FW_NO_UPDATE)
		return -EIO;

	return 0;
}

vb2_error_t vb2ex_auxfw_update(void)
{
	struct vboot_info *vboot = vboot_get();
	struct aux_fw_state state = {0};
	struct udevice *dev;
	int ret;

	uclass_foreach_dev_probe(UCLASS_CROS_AUX_FW, dev) {
		enum aux_fw_severity severity = aux_fw_get_severity(dev);

		if (severity != AUX_FW_NO_UPDATE) {
			ret = do_aux_fw_update(vboot, dev, &state);
			if (ret) {
				log_err("Update for '%s' failed: err=%d\n",
					dev->name, ret);
				break;
			}
		}
		log_info("Protect aux fw '%s'\n", dev->name);
		ret = aux_fw_set_protect(dev, true);
		if (ret) {
			log_err("Update for '%s' failed: err=%d\n", dev->name,
				ret);
			break;
		}
	}
	/* Re-enable power button after update, if required */
	if (state.power_button_disabled)
		cros_ec_config_powerbtn(vboot->cros_ec,
					EC_POWER_BUTTON_ENABLE_PULSE);

	/* Re-enable lid shutdown event, if required */
	if (state.lid_shutdown_disabled)
		cros_ec_set_lid_shutdown_mask(vboot->cros_ec, 1);

	/* Request EC reboot, if required */
	if (state.reboot_required && !ret)
		return VB2_REQUEST_REBOOT_EC_TO_RO;

	return 0;
}

vb2_error_t vb2ex_auxfw_finalize(struct vb2_context *ctx)
{
	/* TODO(sjg@chromium.org): Implement this */
	return 0;
}

