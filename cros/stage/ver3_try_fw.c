// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <log.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>

int vboot_ver3_try_fw(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	bootstage_mark(BOOTSTAGE_VBOOT_START_VERIFY_SLOT);

	log_buffer(UCLASS_TPM, LOGL_INFO, 0, ctx->secdata_kernel, 1, 0x28, 0);

	ret = vb2api_fw_phase3(ctx);
	bootstage_mark(BOOTSTAGE_VBOOT_END_VERIFY_SLOT);
	if (ret) {
		log_info("Reboot reqested (%x)\n", ret);
		return VB2_REQUEST_REBOOT;
	}

	return 0;
}
