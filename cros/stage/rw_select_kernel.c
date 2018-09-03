// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_VBOOT

#include <common.h>
#include <cros_ec.h>
#include <ec_commands.h>
#include <log.h>
#include <mapmem.h>
#include <sysreset.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>

int vboot_rw_select_kernel(struct vboot_info *vboot)
{
	VbSelectAndLoadKernelParams *kparams = &vboot->kparams;
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	fdt_addr_t kaddr;
	fdt_size_t ksize;
	vb2_error_t res;
	int ret;

	ret = vboot_load_config(vboot);
	if (ret)
		return log_msg_ret("config", ret);
	kaddr = ofnode_get_addr_size(vboot->config, "kernel-addr", &ksize);
	if (kaddr == FDT_ADDR_T_NONE)
		return log_msg_ret("kernel address", -EINVAL);
	log_debug("Loading kernel to address %lx\n", (ulong)kaddr);
	kparams->kernel_buffer = map_sysmem(kaddr, ksize);
	kparams->kernel_buffer_size = ksize;

	// On x86 systems, inhibit power button pulse from EC.
	if (IS_ENABLED(CONFIG_X86) && CONFIG_IS_ENABLED(CROS_EC)) {
		ret = cros_ec_config_powerbtn(vboot->cros_ec, 0);
		if (ret)
			log_warning("Failed to configure power button (err=%d)\n",
				    ret);

		/* TODO(sjg@chromium.org): Re-enable before boot */
	}

	/*
	 * TODO(sjg@chromium.org): enable this
	 * if (CONFIG_IS_ENABLED(CROS_EC))
	 *	ctx->flags |= VB2_CONTEXT_EC_SYNC_SUPPORTED;
	 */

	/*
	 * If the lid is closed, kernel selection should not count down the
	 * boot tries for updates, since the OS will shut down before it can
	 * register success.
	 */
	if (vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN) == 0)
		ctx->flags |= VB2_CONTEXT_NOFAIL_BOOT;

	log_debug("Calling VbSelectAndLoadKernel().\n");
	res = VbSelectAndLoadKernel(vboot->ctx, kparams);

	if (res == VB2_REQUEST_REBOOT_EC_TO_RO) {
		printf("EC Reboot requested. Doing cold reboot.\n");
		/*
		 * We could create a sysreset driver for cros_ec and have it
		 * do the reset. But for now this seems sufficient.
		 */
		if (CONFIG_IS_ENABLED(CROS_EC))
			cros_ec_reboot(vboot->cros_ec, EC_REBOOT_COLD,
				       EC_REBOOT_FLAG_ON_AP_SHUTDOWN);
		sysreset_walk_halt(SYSRESET_POWER_OFF);
	} else if (res == VB2_REQUEST_REBOOT_EC_SWITCH_RW) {
		printf("Switch EC slot requested. Doing cold reboot.\n");
		if (CONFIG_IS_ENABLED(CROS_EC))
			cros_ec_reboot(vboot->cros_ec, EC_REBOOT_COLD,
				       EC_REBOOT_FLAG_SWITCH_RW_SLOT);
		sysreset_walk_halt(SYSRESET_POWER_OFF);
	} else if (res == VB2_REQUEST_SHUTDOWN) {
		printf("Powering off.\n");
		sysreset_walk_halt(SYSRESET_POWER_OFF);
	} else if (res == VB2_REQUEST_REBOOT) {
		printf("Reboot requested. Doing warm reboot.\n");
		sysreset_walk_halt(SYSRESET_COLD);
	}
	if (res != VB2_SUCCESS) {
		printf("VbSelectAndLoadKernel returned %#x, "
		       "Doing a cold reboot.\n", res);
		sysreset_walk_halt(SYSRESET_COLD);
	}

	return 0;
}
