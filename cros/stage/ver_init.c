// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <bloblist.h>
#include <cros_ec.h>
#include <ec_commands.h>
#include <dm.h>
#include <log.h>
#include <spl.h>
#include <tpm-common.h>
#include <cros/antirollback.h>
#include <cros/cros_common.h>
#include <cros/nvdata.h>
#include <cros/tpm_common.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>

#include <vb2_internals_please_do_not_use.h>

/**
 * vb2_init_blob() - Set up the vboot persistent blob
 *
 * This holds struct vboot_blob which includes things we need to know and also
 * the vb2_context. Set up a work buffer within the vb2_context.
 *
 * The blob is called persistent since it is preserved through each stage of
 * the boot.
 *
 * @blob: Pointer to the persistent blob for vboot
 * @workbuf_size: Size to use for work buffer
 */
static int vb2_init_blob(struct vboot_blob *blob, int workbuf_size,
			 struct vb2_context **ctxp)
{
	struct vb2_context *ctx;
	int ret;

	/* Initialize vb2_shared_data and friends. */
	ret = vb2api_init((void *)blob, workbuf_size, &ctx);
	if (ret)
		return log_msg_ret("init_context", ret);
	*ctxp = ctx;

	return 0;
}

int vboot_ver_init(struct vboot_info *vboot)
{
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	int ret;

	printf("vboot starting in %s\n", spl_phase_name(spl_phase()));
	log_debug("vboot is at %p, size %lx, bloblist %p\n", vboot,
		  (ulong)sizeof(*vboot), gd->bloblist);
	blob = bloblist_add(BLOBLISTT_VBOOT_CTX, sizeof(struct vboot_blob),
			    VBOOT_CONTEXT_ALIGN);
	if (!blob)
		return log_msg_ret("blob", -ENOSPC);

	bootstage_mark(BOOTSTAGE_VBOOT_START);

	ret = vboot_load_config(vboot);
	if (ret)
		return log_msg_ret("load config", ret);
	/* Set up context and work buffer */
	ret = vb2_init_blob(blob, VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE, &ctx);
	if (ret)
		return log_msg_ret("set up work context", ret);
	vboot->blob = blob;
	vboot->ctx = ctx;
	ctx->non_vboot_context = vboot;
	vboot->valid = true;

	ret = uclass_get_device_by_seq(UCLASS_TPM, 0, &vboot->tpm);
	if (ret)
		ret = uclass_first_device_err(UCLASS_TPM, &vboot->tpm);
	if (ret)
		return log_msg_ret("find TPM", ret);
	log_info("TPM: %s, version %s\n", vboot->tpm->name,
		tpm_get_version(vboot->tpm) == TPM_V1 ? "v1.2" : "v2");

	/*
	 * Read secdata from TPM. Initialise TPM if secdata not found. We don't
	 * check the return value here because vb2api_fw_phase1 will catch
	 * invalid secdata and tell us what to do (=reboot).
	 */
	bootstage_mark(BOOTSTAGE_VBOOT_START_TPMINIT);
	ret = vboot_setup_tpm(vboot);
	if (ret) {
		log_err("TPM setup failed (err=%x)\n", ret);
	} else {
		ret = antirollback_read_space_firmware(vboot);
		/*
		 * This indicates a coding error (e.g. not supported in TPM
		 * emulator, so fail immediately.
		 */
		if (ret == -ENOSYS)
			return log_msg_ret("inval", ret);
		antirollback_read_space_kernel(vboot);
	}
	bootstage_mark(BOOTSTAGE_VBOOT_END_TPMINIT);

	/* initialise and read nvdata from non-volatile storage */
	ret = cros_nvdata_read_walk(CROS_NV_DATA, ctx->nvdata,
				    VB2_NVDATA_SIZE_V2);
	if (ret)
		return log_msg_ret("read nvdata", ret);

	/* Dump all the context */
	vboot_nvdata_dump(ctx->nvdata, VB2_NVDATA_SIZE_V2);
	vboot_secdataf_dump(ctx->secdata_firmware,
			    sizeof(ctx->secdata_firmware));
	vboot_secdatak_dump(ctx->secdata_kernel, sizeof(ctx->secdata_kernel));

	ret = cros_ofnode_flashmap(&vboot->fmap);
	if (ret)
		return log_msg_ret("failed to decode fmap\n", ret);
	cros_ofnode_dump_fmap(&vboot->fmap);
	ret = uclass_first_device_err(UCLASS_CROS_FWSTORE, &vboot->fwstore);
	if (ret)
		return log_msg_ret("fwstore", ret);

	if (CONFIG_IS_ENABLED(CROS_EC)) {
		ret = uclass_get_device(UCLASS_CROS_EC, 0, &vboot->cros_ec);
		if (ret)
			return log_msg_ret("locate Chromium OS EC", ret);

		/*
		 * Allow for special key combinations on sandbox, e.g. to enter
		 * recovery mode
		 */
		if (IS_ENABLED(CONFIG_SANDBOX))
			cros_ec_check_keyboard(vboot->cros_ec);
	}
	/*
	 * Set S3 resume flag if vboot should behave differently when selecting
	 * which slot to boot.  This is only relevant to vboot if the platform
	 * does verification of memory init and thus must ensure it resumes with
	 * the same slot that it booted from.
	 */
	if (vboot->resume_path_same_as_boot && !vboot->meminit_in_ro &&
	    vboot_platform_is_resuming())
		ctx->flags |= VB2_CONTEXT_S3_RESUME;

	if (vboot_flag_read_walk(VBOOT_FLAG_RECOVERY) == 1) {
		ctx->flags |= VB2_CONTEXT_FORCE_RECOVERY_MODE;
		if (vboot->disable_dev_on_rec)
			ctx->flags |= VB2_CONTEXT_DISABLE_DEVELOPER_MODE;
		log_info("Enabled recovery mode\n");
	}

	if (vboot_flag_read_walk(VBOOT_FLAG_WIPEOUT) == 1)
		ctx->flags |= VB2_CONTEXT_FORCE_WIPEOUT_MODE;
	if (vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN) == 0)
		ctx->flags |= VB2_CONTEXT_NOFAIL_BOOT;
	ctx->flags |= VB2_CONTEXT_NVDATA_V2;
	ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;

	return 0;
}
