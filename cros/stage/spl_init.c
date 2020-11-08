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

#include <tpm-common.h>

int vboot_spl_init(struct vboot_info *vboot)
{
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	int ret;

	blob = bloblist_find(BLOBLISTT_VBOOT_CTX, sizeof(*blob));
	if (!blob)
		return log_msg_ret("Cannot find bloblist", -ENOENT);
	vboot->blob = blob;
	ctx = &blob->ctx;
	vboot->ctx = ctx;
	ctx->non_vboot_context = vboot;
	vboot->valid = true;

	ret = uclass_first_device_err(UCLASS_CROS_FWSTORE, &vboot->fwstore);
	if (ret)
		return log_msg_ret("Cannot set up fwstore", ret);

	/* Probe the EC so that it will read and write its state */
	if (IS_ENABLED(CONFIG_SANDBOX) && CONFIG_IS_ENABLED(CROS_EC))
		uclass_get_device(UCLASS_CROS_EC, 0, &vboot->cros_ec);

	if (1) {
		u8 sendbuf[] = {
			0x80, 0x02, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00,
			0x01, 0x4e, 0x40, 0x00, 0x00, 0x0c, 0x01, 0x00,
			0x10, 0x08, 0x00, 0x00, 0x00, 0x09, 0x40, 0x00,
			0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x0d, 0x00, 0x00
		};
		struct udevice *tpm;
		u8 recvbuf[100];
		size_t recv_size;
		int ret;

		ret = uclass_first_device_err(UCLASS_TPM, &tpm);
		if (ret)
			return log_msg_ret("Cannot locate TPM", ret);

		recv_size = 100;
		ret = tpm_xfer(tpm, sendbuf, 0x23, recvbuf, &recv_size);
		printf("\ntry ret=%d\n\n", ret);
		while (ret);
	}

	return 0;
}
