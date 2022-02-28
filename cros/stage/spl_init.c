// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bloblist.h>
#include <dm.h>
#include <log.h>
#include <cros/vboot.h>

int vboot_spl_init(struct vboot_info *vboot)
{
	struct vboot_blob *blob;
	int ret;

	blob = bloblist_find(BLOBLISTT_VBOOT_CTX, sizeof(*blob));
	if (!blob)
		return log_msg_ret("ctx", -ENOENT);
	ret = vb2api_reinit(blob, &vboot->ctx);
	if (ret)
		return log_msg_ret("blob", ret);
	vboot->blob = blob;
	vboot->ctx->non_vboot_context = vboot;
	vboot->valid = true;

	ret = uclass_first_device_err(UCLASS_CROS_FWSTORE, &vboot->fwstore);
	if (ret)
		return log_msg_ret("fwstore", ret);

	/* Probe the EC so that it will read and write its state */
	if (IS_ENABLED(CONFIG_SANDBOX) && CONFIG_IS_ENABLED(CROS_EC))
		uclass_get_device(UCLASS_CROS_EC, 0, &vboot->cros_ec);

	return 0;
}
