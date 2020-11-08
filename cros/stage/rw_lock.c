
// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>

int vboot_rw_lock(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	/* This should be done on resume */
	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
		ret = cros_nvdata_lock_walk(CROS_NV_SECDATAK);
		if (ret) {
			log_info("Failed to lock TPM (%x)\n", ret);
			vb2api_fail(ctx, VB2_RECOVERY_RO_TPM_L_ERROR, 0);
			return VBERROR_REBOOT_REQUIRED;
		}
	} else {
		log_info("Not locking secdata_kernel in recovery mode\n");
		return 0;
	}

	return 0;
}
