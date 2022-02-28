// SPDX-License-Identifier: GPL-2.0+
/*
 * General functions used by vboot implementation
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <cros_ec.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <mapmem.h>
#include <asm/io.h>
#include <cros/vboot.h>

int vboot_alloc(struct vboot_info **vbootp)
{
	gd->vboot = calloc(1, sizeof(struct vboot_info));
	if (!gd->vboot) {
		log_err("Cannot allocate vboot %x\n",
                        (uint)sizeof(struct vboot_info));
		return -ENOMEM;
	}
	*vbootp = gd->vboot;

	return 0;
}

struct vboot_info *vboot_get(void)
{
	struct vboot_info *vboot = gd->vboot;

	return vboot->valid ? vboot : NULL;
}

struct vboot_info *vboot_get_alloc(void)
{
	struct vboot_info *vboot = gd->vboot;

	if (!vboot)
		vboot_alloc(&vboot);

	return vboot;
}

int vboot_load_config(struct vboot_info *vboot)
{
	ofnode node;

	node = cros_ofnode_config_node();
	if (!ofnode_valid(node))
		return -ENOENT;

	vboot->deactivate_tpm = ofnode_read_bool(node, "deactivate-tpm");
	vboot->disable_dev_on_rec = ofnode_read_bool(node,
						     "disable-dev-on-rec");
	vboot->ec_efs = ofnode_read_bool(node, "ec-efs");
	vboot->ec_slow_update = ofnode_read_bool(node, "ec-slow-update");
	vboot->ec_software_sync = ofnode_read_bool(node, "ec-software-sync");
	vboot->has_rec_mode_mrc = ofnode_read_bool(node, "recovery-mode-mrc");
	vboot->meminit_in_ro = ofnode_read_bool(node,
						"meminit-in-readonly-code");
	vboot->oprom_matters = ofnode_read_bool(node, "oprom-matters");
	vboot->physical_dev_switch = ofnode_read_bool(node,
						      "physical-dev-switch");
	vboot->physical_rec_switch = ofnode_read_bool(node,
						      "physical-rec-switch");
	vboot->resume_path_same_as_boot = ofnode_read_bool(node,
						"resume-path-same-as-boot");
#ifndef CONFIG_SPL_BUILD
	vboot->detachable_ui = ofnode_read_bool(node, "detachable-ui");
	vboot->disable_memwipe = ofnode_read_bool(node, "disable-memwipe");
	vboot->disable_lid_shutdown_during_update = ofnode_read_bool(node,
					"disable-lid-shutdown-during-update");
	vboot->disable_power_button_during_update = ofnode_read_bool(node,
					"disable-power-button-during-update");
#endif

	vboot->config = node;

	return 0;
}

bool vboot_is_slot_a(const struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return !(ctx->flags & VB2_CONTEXT_FW_SLOT_B);
}

bool vboot_is_recovery(const struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return ctx->flags & VB2_CONTEXT_RECOVERY_MODE;
}

const char *vboot_slot_name(const struct vboot_info *vboot)
{
	return vboot_is_slot_a(vboot) ? "A" : "B";
}

struct fmap_section *vboot_get_section(struct vboot_info *vboot,
				       bool *is_rwp)
{
	if (vboot_is_recovery(vboot)) {
		*is_rwp = false;
		return &vboot->fmap.readonly;
	}
	*is_rwp = true;
	if (vboot_is_slot_a(vboot))
		return &vboot->fmap.readwrite_a;
	else
		return &vboot->fmap.readwrite_b;
}

void vboot_set_selected_region(struct vboot_info *vboot,
			       const struct fmap_entry *spl,
			       const struct fmap_entry *u_boot)
{
	vboot->blob->spl_entry = *spl;
	vboot->blob->u_boot_entry = *u_boot;
}

int vboot_platform_is_resuming(void)
{
	/* TODO(sjg@chromium.org): Implement this */

	return 0;
}
