// SPDX-License-Identifier: GPL-2.0+
/*
 * Sets up the read-write vboot portion (which loads the kernel)
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <bloblist.h>
#include <cbfs.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <mapmem.h>
#include <tpm-common.h>
#include <cros/cb_helper.h>
#include <cros/cros_ofnode.h>
#include <cros/fmap.h>
#include <cros/fwstore.h>
#include <cros/keyboard.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>
#include <cros/memwipe.h>

#include <vb2_internals_please_do_not_use.h>

/**
 * setup_unused_memory() - find memory to clear
 *
 * @vboot: vboot context
 * @wipe: Information about memory to wipe
 */
static void setup_unused_memory(struct vboot_info *vboot, struct memwipe *wipe)
{
	struct fdt_memory ramoops;
	int bank;

	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		if (!gd->bd->bi_dram[bank].size)
			continue;
		memwipe_add(wipe, gd->bd->bi_dram[bank].start,
			    gd->bd->bi_dram[bank].start +
			    gd->bd->bi_dram[bank].size);
	}

	/* Excludes kcrashmem if in FDT */
	if (cros_ofnode_memory("/ramoops", &ramoops))
		log_debug("RAMOOPS not contained within FDT\n");
	else
		memwipe_sub(wipe, ramoops.start, ramoops.end);
}

/**
 * get_current_sp() - Get an approximation of the stack pointer
 *
 * @return current stack-pointer value
 */
static ulong get_current_sp(void)
{
#ifdef CONFIG_SANDBOX
	return gd->start_addr_sp;
#else
	ulong addr, sp;

	sp = (ulong)&addr;

	return sp;
#endif
}

/**
 * memory_wipe_unused() - Wipe memory not needed to boot
 *
 * This provides additional security by clearing out memory that might contain
 * things from a previous boot
 *
 * @vboot: vboot context
 * @return 0 if OK, -EPERM if coreboot tables are needed but missing on x86
 *	(fatal error)
 */
static int memory_wipe_unused(struct vboot_info *vboot)
{
	struct memwipe wipe;

	memwipe_init(&wipe);
	if (vboot_from_cb(vboot)) {
		int ret;

		ret = cb_setup_unused_memory(vboot, &wipe);
		if (ret)
			return log_msg_ret("wipe", ret);
	} else {
		setup_unused_memory(vboot, &wipe);
	}

	/* Exclude relocated U-Boot structures */
	memwipe_sub(&wipe, get_current_sp() - MEMWIPE_STACK_MARGIN,
		    gd->ram_top);

	memwipe_execute(&wipe);

	return 0;
}

static int vboot_check_wipe_memory(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	if (ctx->flags & VB2_CONTEXT_CLEAR_RAM) {
		if (vboot->disable_memwipe)
			log_warning("Memory wipe requested but not supported\n");
		else
			return memory_wipe_unused(vboot);
	}

	return 0;
}


int vboot_rw_init(struct vboot_info *vboot)
{
	struct fmap_section *section;
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	bool is_rw;
	int ret;

	if (!IS_ENABLED(CONFIG_SYS_COREBOOT) || ll_boot_init()) {
		int new_size = VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE;
		int ret;

		blob = bloblist_find(BLOBLISTT_VBOOT_CTX, sizeof(*blob));
		if (!blob)
			return log_msg_ret("blob", -ENOENT);
		ret = bloblist_resize(BLOBLISTT_VBOOT_CTX, new_size);
		if (ret)
			return log_msg_ret("resize", ret);

		ret = vb2api_relocate(blob, blob, new_size, &ctx);
		if (ret)
			return log_msg_ret("reloc", ret);

		log_warning("flags %llx %d\n", ctx->flags,
			    ((ctx->flags & VB2_CONTEXT_RECOVERY_MODE) != 0));
	} else {
		ret = cb_vboot_rw_init(vboot, &ctx);
		if (ret)
			return log_msg_ret("cb", ret);
	}
	vboot->ctx = ctx;
	ctx->non_vboot_context = vboot;

	ret = vboot_check_wipe_memory(vboot);
	if (ret)
		log_warning("Failed to wipe memory (err=%d)\n", ret);

	if (vboot_is_recovery(vboot))
		log_info("Recovery mode\n");
	else
		log_info("Booting from slot %s: vboot->ctx=%p, flags %llx\n",
			 vboot_slot_name(vboot), vboot->ctx, vboot->ctx->flags);
	vboot->valid = true;

	ret = vboot_load_config(vboot);
	if (ret)
		return log_msg_ret("cfg", ret);

	ret = uclass_first_device_err(UCLASS_TPM, &vboot->tpm);
	if (ret)
		return log_msg_ret("tpm", ret);

	ret = uclass_first_device_err(UCLASS_CROS_FWSTORE, &vboot->fwstore);
	if (ret)
		return log_msg_ret("fwstore", ret);

	section = vboot_get_section(vboot, &is_rw);

	if (!vboot_from_cb(vboot)) {
		ret = cros_ofnode_flashmap(&vboot->fmap);
		if (ret)
			return log_msg_ret("ofmap", ret);
	} else {
		ret = cb_setup_flashmap(vboot);
		if (ret)
			return log_msg_ret("cbmap", ret);
	}
	cros_ofnode_dump_fmap(&vboot->fmap);

	ret = vboot_keymap_init(vboot);
	if (ret)
		return log_msg_ret("key remap", ret);

	ret = cros_fwstore_read_entry_raw(vboot->fwstore,
					  &vboot->fmap.readonly.firmware_id,
					  &vboot->readonly_firmware_id,
					  sizeof(vboot->readonly_firmware_id));
	if (ret)
		return log_msg_ret("ro", ret);

	ret = cros_fwstore_read_entry_raw(vboot->fwstore,
					  &section->firmware_id,
					  &vboot->firmware_id,
					  sizeof(vboot->firmware_id));
	if (ret)
		return log_msg_ret("rw", ret);

	if (CONFIG_IS_ENABLED(CROS_EC)) {
		ret = uclass_get_device(UCLASS_CROS_EC, 0, &vboot->cros_ec);
		if (ret)
			return log_msg_ret("ec", ret);
	}

	/* initialise and read fwmp from TPM */
	ret = cros_nvdata_read_walk(CROS_NV_FWMP, ctx->secdata_fwmp,
				    VB2_SECDATA_FWMP_MIN_SIZE);
	if (ret)
		return log_msg_ret("read nvdata", ret);
	vboot_fwmp_dump(ctx->secdata_fwmp, VB2_SECDATA_FWMP_MIN_SIZE);

	return 0;
}
