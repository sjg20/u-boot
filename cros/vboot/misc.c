// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of misc callbacks
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <gpt.h>
#include <log.h>
#include <malloc.h>
#include <vb2_api.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>

u32 VbExIsShutdownRequested(void)
{
	int ret, prev;

	/* if lid is NOT OPEN */
	ret = vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN);
	if (ret == 0) {
		log_info("Lid-closed is detected\n");
		return 1;
	}
	/*
	 * If power switch is pressed (but previously was known to be not
	 * pressed), we power off.
	 */
	ret = vboot_flag_read_walk_prev(VBOOT_FLAG_POWER_BUTTON, &prev, NULL);
	if (!ret && prev == 1) {
		log_info("Power-key-pressed is detected\n");
		return 1;
	}
	/*
	 * Either the gpios don't exist, or the lid is up and and power button
	 * is not pressed. No-Shutdown-Requested.
	 */
	return 0;
}

vb2_error_t vb2ex_commit_data(struct vb2_context *ctx)
{
	struct vboot_info *vboot = ctx_to_vboot(ctx);
	vb2_error_t vberr;
	int ret;

	ret = vboot_save_if_needed(vboot, &vberr);
	if (ret) {
		if (vberr == VB2_ERROR_NV_WRITE) {
			log_err("write nvdata returned %#x\n", ret);
			/*
			 * We can't write to nvdata, so it's impossible to
			 * trigger * recovery mode.  Skip calling vb2api_fail()
			 * and just die.
			 */
			if (!vboot_is_recovery(vboot))
			        panic("can't write recovery reason to nvdata");
			/*
			 * If we *are* in recovery mode, ignore any error and
			 * return
			 */
			return VB2_SUCCESS;
			}

		return vberr;
	}

	return VB2_SUCCESS;
}

int vb2ex_physical_presence_pressed(void)
{
	return 0;
}

void *vbex_malloc(size_t size)
{
	return malloc(size);
}
