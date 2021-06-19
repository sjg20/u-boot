// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of TPM callbacks
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY	UCLASS_TPM

#include <common.h>
#include <config.h>
#include <tpm-v1.h>
#include <cros/vboot.h>

vb2_error_t vb2ex_tpm_init(void)
{
	return vb2ex_tpm_open();
}

vb2_error_t vb2ex_tpm_close(void)
{
	struct vboot_info *vboot = vboot_get();

	if (tpm_close(vboot->tpm))
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_tpm_open(void)
{
	struct vboot_info *vboot = vboot_get();

	if (tpm_open(vboot->tpm))
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_tpm_send_recv(const u8 *request, u32 request_length,
				u8 *response, u32 *response_length)
{
	struct vboot_info *vboot = vboot_get();
	size_t resp_len = *response_length;
	int ret;

	/*
	 * Use the low-level U-Boot API directory. This transfers bytes back and
	 * forth but all the message assembly and decoding happens in the vboot
	 * library.
	 */
	ret = tpm_xfer(vboot->tpm, request, request_length, response,
		       &resp_len);
	*response_length = resp_len;
	if (ret)
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}
