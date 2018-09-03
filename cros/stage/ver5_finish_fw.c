// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <bloblist.h>
#include <log.h>
#include <cros/cros_common.h>
#include <cros/tpm_common.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>

int vboot_ver5_finish_fw(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	struct cros_fmap *fmap;
	int ret;

	bootstage_mark(BOOTSTAGE_VBOOT_START_TPMPCR);
	ret = vboot_extend_pcrs(vboot);
	if (ret) {
		log_warning("Failed to extend TPM PCRs (%d)\n", ret);
		vb2api_fail(ctx, VB2_RECOVERY_RO_TPM_U_ERROR, ret);
		return VB2_REQUEST_REBOOT;
	}
	bootstage_mark(BOOTSTAGE_VBOOT_END_TPMPCR);

	/* Lock TPM */
	bootstage_mark(BOOTSTAGE_VBOOT_START_TPMLOCK);
	ret = cros_nvdata_lock_walk(CROS_NV_SECDATAF);
	if (ret) {
		log_info("Failed to lock TPM (%x)\n", ret);
		vb2api_fail(ctx, VB2_RECOVERY_RO_TPM_L_ERROR, 0);
		return VB2_REQUEST_REBOOT;
	}
	bootstage_mark(BOOTSTAGE_VBOOT_END_TPMLOCK);

	/* Lock rec hash space if available */
	if (vboot->has_rec_mode_mrc) {
		ret = cros_nvdata_lock_walk(CROS_NV_MRC_REC_HASH);
		if (ret) {
			log_info("Failed to lock rec hash space(%x)\n", ret);
			vb2api_fail(ctx, VB2_RECOVERY_RO_TPM_REC_HASH_L_ERROR,
				    0);
			return VB2_REQUEST_REBOOT;
		}
	}

	log_info("Slot %c is selected\n", vboot_is_slot_a(vboot) ? 'A' : 'B');
	fmap = &vboot->fmap;
	vboot_set_selected_region(vboot, vboot_is_slot_a(vboot) ?
		&fmap->readwrite_a.spl : &fmap->readwrite_b.spl,
		vboot_is_slot_a(vboot) ?
		&fmap->readwrite_a.boot : &fmap->readwrite_b.boot);
	bloblist_finish();
	bootstage_mark(BOOTSTAGE_VBOOT_END);

	return 0;
}
