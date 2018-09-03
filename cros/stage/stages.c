// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified boot stages
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <errno.h>
#include <log.h>
#include <spl.h>
#include <sysreset.h>
#include <cros/nvdata.h>
#include <cros/stages.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>

struct vboot_stage {
	const char *name;
	int (*run)(struct vboot_info *vboot);
};

#if CONFIG_IS_ENABLED(CHROMEOS_VBOOT_A) || \
	CONFIG_IS_ENABLED(CHROMEOS_VBOOT_B) || \
	CONFIG_IS_ENABLED(CHROMEOS_VBOOT_C)

/*
 * There are three groups here. TPL or VPL runs the early firmware-selection
 * process, then SPL sets up SDRAM and jumps to U-Boot proper, which does the
 * kernel-selection process. We only build in the code that is actually needed
 * by each stage.
 *
 * For testing it might be convenient to include all stages in one binary, but
 * so far it seems easy enough to run gdb and have it follow the processes.
 */
struct vboot_stage stages[VBOOT_STAGE_COUNT] = {
#if CONFIG_IS_ENABLED(CHROMEOS_VBOOT_A)
	/* Verification stage: figures out which firmware to run */
	[VBOOT_STAGE_VER_INIT] = {"ver_init", vboot_ver_init},
	[VBOOT_STAGE_VER1_VBINIT] = {"ver1_vbinit", vboot_ver1_vbinit},
	[VBOOT_STAGE_VER2_SELECTFW] = {"ver2_selectfw", vboot_ver2_select_fw,},
	[VBOOT_STAGE_VER3_TRYFW] = {"ver3_tryfw", vboot_ver3_try_fw,},
	[VBOOT_STAGE_VER4_LOCATEFW] = {"ver4_locatefw", vboot_ver4_locate_fw,},
	[VBOOT_STAGE_VER_FINISH] = {"ver5_finishfw", vboot_ver5_finish_fw,},
	[VBOOT_STAGE_VER_JUMP] = {"ver_jump", vboot_ver6_jump_fw,},
#endif
#if CONFIG_IS_ENABLED(CHROMEOS_VBOOT_B)
	/* SPL stage: Sets up SDRAM and jumps to U-Boot proper */
	[VBOOT_STAGE_RW_SPL_INIT] = {"spl_init", vboot_spl_init,},
	[VBOOT_STAGE_RW_SPL_JUMP_U_BOOT] =
		{"spl_jump_u_boot", vboot_spl_jump_u_boot,},
	[VBOOT_STAGE_RW_INIT] = {},
#endif
#if CONFIG_IS_ENABLED(CHROMEOS_VBOOT_C)
	/* U-Boot stage: Boots the kernel */
	[VBOOT_STAGE_RW_INIT] = {"rw_init", vboot_rw_init,},
	[VBOOT_STAGE_RW_SELECTKERNEL] =
		{"rw_selectkernel", vboot_rw_select_kernel,},
	[VBOOT_STAGE_RW_BOOTKERNEL] = {"rw_bootkernel", vboot_rw_boot_kernel,},
#endif

#if 0
	/* For VB2, when supported */
	[VBOOT_STAGE_RW_KERNEL_PHASE1] =
		{"rw_kernel_phase1", vboot_rw_kernel_phase1,},
	[VBOOT_STAGE_RW_KERNEL_PHASE2] =
		{"rw_kernel_phase2", vboot_rw_kernel_phase2,},
	[VBOOT_STAGE_RW_KERNEL_PHASE3] =
		{"rw_kernel_phase3", vboot_rw_kernel_phase3,},
	[VBOOT_STAGE_RW_KERNEL_BOOT] =
		{ "rw_kernel_boot", vboot_rw_kernel_boot, },
#endif
};

const char *vboot_get_stage_name(enum vboot_stage_t stagenum)
{
	if (stagenum >= VBOOT_STAGE_FIRST && stagenum < VBOOT_STAGE_COUNT)
		return stages[stagenum].name ?: "(unknown)";

	return "(invalid)";
}

enum vboot_stage_t vboot_find_stage(const char *name)
{
	enum vboot_stage_t stagenum;

	for (stagenum = VBOOT_STAGE_FIRST; stagenum < VBOOT_STAGE_COUNT;
	     stagenum++) {
		struct vboot_stage *stage = &stages[stagenum];

		if (!strcmp(name, stage->name))
			return stagenum;
	}

	return VBOOT_STAGE_NONE;
}

int vboot_run_stage(struct vboot_info *vboot, enum vboot_stage_t stagenum)
{
	struct vboot_stage *stage = &stages[stagenum];
	int ret;

	log_info("Running stage '%s'\n", stage->name);
	if (!stage->run) {
		log_debug("   - Stage '%s' not available\n", stage->name);
		return -EPERM;
	}

	bootstage_mark_name(BOOTSTAGE_VBOOT_FIRST + stagenum, stage->name);
	ret = (*stage->run)(vboot);
	if (ret)
		log_err("Error: stage '%s' returned %x\n", stage->name, ret);

	return ret;
}

/**
 * save_if_needed() - Save non-volatile and/or secure data if it has changed
 *
 * @vboot: vboot context
 * @return 0 if OK, -ve if save failed
 */
static int save_if_needed(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	if (!ctx)
		return -ENOENT;
	if (ctx->flags & VB2_CONTEXT_NVDATA_CHANGED) {
		log_info("Saving nvdata\n");

		log_buffer(LOGC_VBOOT, LOGL_DEBUG, 0, ctx->nvdata, 1,
			   sizeof(ctx->nvdata), 0);
		if (spl_phase() != PHASE_SPL)
			vboot_dump_nvdata(ctx->nvdata, sizeof(ctx->nvdata));
		ret = cros_nvdata_write_walk(CROS_NV_DATA, ctx->nvdata,
					     sizeof(ctx->nvdata));
		if (ret)
			return log_msg_ret("save nvdata", ret);
		ctx->flags &= ~VB2_CONTEXT_NVDATA_CHANGED;
	}

	if (ctx->flags & VB2_CONTEXT_SECDATA_CHANGED) {
		log_info("Saving secdata\n");
		ret = cros_nvdata_write_walk(CROS_NV_SECDATA, ctx->secdata,
					     sizeof(ctx->secdata));
		if (ret) {
			return log_msg_ret("secdata", ret);
		}
		ctx->flags &= ~VB2_CONTEXT_SECDATA_CHANGED;
	}

	if (ctx->flags & VB2_CONTEXT_SECDATAK_CHANGED) {
		log_info("Saving secdatak\n");
		ret = cros_nvdata_write_walk(CROS_NV_SECDATAK, ctx->secdatak,
					     sizeof(ctx->secdatak));
		if (ret)
			return log_msg_ret("secdatak", ret);
		ctx->flags &= ~VB2_CONTEXT_SECDATAK_CHANGED;
	}

	return 0;
}

int vboot_run_stages(struct vboot_info *vboot, enum vboot_stage_t start,
		     uint flags)
{
	enum vboot_stage_t stagenum;
	int ret = 0;

	for (stagenum = start; !ret && stagenum < VBOOT_STAGE_COUNT;
	     stagenum++) {
		if (!stages[stagenum].name)
			break;
		ret = vboot_run_stage(vboot, stagenum);
		save_if_needed(vboot);

		if (stagenum == VBOOT_STAGE_VER1_VBINIT &&
		    ret == VB2_ERROR_API_PHASE1_RECOVERY) {
			struct fmap_section *fw = &vboot->fmap.readonly;
			struct vb2_context *ctx = vboot_get_ctx(vboot);

			vboot_set_selected_region(vboot, &fw->spl_rec, &fw->boot_rec);
			log_warning("flags %x recovery=%d\n", ctx->flags,
				    (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) != 0);
			ret = 0;
		}
	}

	/* Success - ready to continue */
	if (!ret)
		return 0;

	if (CONFIG_IS_ENABLED(SYS_MALLOC_SIMPLE))
		malloc_simple_info();

	/* Allow dropping to the command line here for debugging */
	if (flags & VBOOT_FLAG_CMDLINE)
		return -EPERM;

	if (ret == VBERROR_REBOOT_REQUIRED) {
		log_warning("Cold reboot\n");
		sysreset_walk_halt(SYSRESET_COLD);
	} else {
		switch (vboot->vb_error) {
		case VBERROR_BIOS_SHELL_REQUESTED:
			return -EPERM;
		case VBERROR_EC_REBOOT_TO_RO_REQUIRED:
		case VBERROR_SHUTDOWN_REQUESTED:
			log_warning("Power off\n");
			sysreset_walk_halt(SYSRESET_POWER_OFF);
			break;
		default:
			log_warning("Cold reboot\n");
			sysreset_walk_halt(SYSRESET_COLD);
			break;
		}
	}

	/* Not reached */
	sysreset_walk_halt(SYSRESET_COLD);

	return 0;
}

int vboot_run_auto(struct vboot_info *vboot, uint flags)
{
	enum vboot_stage_t stage;

	log_debug("start\n");

	if (CONFIG_IS_ENABLED(CHROMEOS_VBOOT_A))
		stage = VBOOT_STAGE_FIRST_VER;
	else if (CONFIG_IS_ENABLED(CHROMEOS_VBOOT_B))
		stage = VBOOT_STAGE_RW_FIRST_SPL;
	else
		stage = VBOOT_STAGE_RW_FIRST_U_BOOT;

	return vboot_run_stages(vboot, stage, flags);
}

/**
 * board_boot_order() - Provide a boot order for SPL
 *
 * This tells U-Boot which boot device to use. For now we allow falling back to
 * a secondary boot for testing / development purposes.
 */
void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_CROS_VBOOT;
#ifdef CONFIG_X86
	spl_boot_list[1] = BOOT_DEVICE_SPI_MMAP;
#else
	spl_boot_list[1] = BOOT_DEVICE_BOARD;
#endif
}

int cros_do_stage(void)
{
	struct vboot_info *vboot;
	int ret;

	log_info("start\n");
	if (!CONFIG_IS_ENABLED(CHROMEOS_VBOOT_A))
		return 0;

	ret = vboot_alloc(&vboot);
	if (ret)
		return ret;

	ret = vboot_run_auto(vboot, 0);
	if (ret)
		log_err("VPL error %d\n", ret);

	return 0;
}

#endif /* CHROMEOS_VBOOT_A B or C */

#ifdef CONFIG_VPL_BUILD
/* loader for VPL if enabled */
static int cros_load_image_vpl(struct spl_image_info *spl_image,
			       struct spl_boot_device *bootdev)
{
	struct vboot_info *vboot;
	int ret;

	ret = vboot_alloc(&vboot);
	if (ret)
		return ret;
	vboot->spl_image = spl_image;

	ret = vboot_run_auto(vboot, 0);
	if (ret)
		log_err("VPL error %d\n", ret);
	log_info("Completed loading image\n");

	return 0;
}
SPL_LOAD_IMAGE_METHOD("chromium_vboot_vpl", 0, BOOT_DEVICE_CROS_VBOOT,
		      cros_load_image_vpl);

#elif defined(CONFIG_TPL_BUILD)

/* Nothing in TPL at present */

#elif defined(CONFIG_SPL_BUILD)
/* loader for SPL */
static int cros_load_image_spl(struct spl_image_info *spl_image,
			       struct spl_boot_device *bootdev)
{
	struct vboot_info *vboot;
	int ret;

	ret = vboot_alloc(&vboot);
	if (ret)
		return ret;
	vboot->spl_image = spl_image;

	ret = vboot_run_auto(vboot, 0);
	if (ret)
		return log_msg_ret("vboot", ret);
	log_info("Completed loading image\n");

	return 0;
}
SPL_LOAD_IMAGE_METHOD("chromium_vboot_spl", 0, BOOT_DEVICE_CROS_VBOOT,
		      cros_load_image_spl);
#endif /* CONFIG_SPL_BUILD */
