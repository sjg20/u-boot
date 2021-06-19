// SPDX-License-Identifier: GPL-2.0+
/*
 * Holds functions that need access to internal vboot data
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <cros/vboot.h>
#include <vb2_internals_please_do_not_use.h>

bool vboot_wants_oprom(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return ctx->nvdata[VB2_NV_OFFS_BOOT] & VB2_NV_BOOT_DISPLAY_REQUEST;
}

#ifndef CONFIG_SPL_BUILD
u32 vboot_get_gbb_flags(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return vb2api_gbb_get_flags(ctx);
}
#endif
