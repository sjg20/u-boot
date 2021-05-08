// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <log.h>
#include <cros/cros_common.h>
#include <cros/tpm_common.h>
#include <cros/vboot.h>

int vboot_ver1_vbinit(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	ret = vb2api_fw_phase1(ctx);
	if (ret) {
		log_warning("ret=%x\n", ret);
		/*
		 * If vb2api_fw_phase1 fails, check for return value.
		 * If it is set to VB2_ERROR_API_PHASE1_RECOVERY, then continue
		 * into recovery mode.
		 * For any other error code, save context if needed and reboot.
		 */
		if (ret == VB2_ERROR_API_PHASE1_RECOVERY) {
			log_warning("Recovery requested (%x)\n", ret);
			vboot_extend_pcrs(vboot);	/* ignore failures */
			bootstage_mark(BOOTSTAGE_VBOOT_END);
			return ret;
		}

		log_warning("Reboot reqested (%x)\n", ret);
		return VB2_REQUEST_REBOOT;
	}

	return 0;
}
