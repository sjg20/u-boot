// SPDX-License-Identifier: GPL-2.0+
/*
 * Sets up the read-write vboot portion (which loads the kernel)
 *
 * Copyright 2018 Google LLC
 */

#define NEED_VB20_INTERNALS

#include <common.h>
#include <bloblist.h>
#include <cbfs.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <mapmem.h>
#include <cros/cb_helper.h>
#include <cros/cros_ofnode.h>
#include <cros/fmap.h>
#include <cros/fwstore.h>
#include <cros/keyboard.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>
#include <cros/memwipe.h>

#include <gbb_header.h>
#include <vboot_struct.h>

#include <tpm-common.h>

/**
 * gbb_copy_in() - Copy a portion of the GBB into vboot->cparams
 *
 * @vboot:	Vboot info
 * @gbb_offset:	Offset of GBB in vboot->fwstore
 * @offset:	Offset within GBB to read
 * @size:	Number of bytes to read
 * @return 0 if OK, -EINVAL if offset/size invalid, other error if fwstore
 *	fails to read
 */
static int gbb_copy_in(struct vboot_info *vboot, uint gbb_offset, uint offset,
		       uint size)
{
	VbCommonParams *cparams = &vboot->cparams;
	u8 *gbb_copy = cparams->gbb_data;
	int ret;

	if (offset > cparams->gbb_size || offset + size > cparams->gbb_size)
		return log_msg_ret("range", -EINVAL);
	ret = cros_fwstore_read(vboot->fwstore, gbb_offset + offset, size,
				gbb_copy + offset);
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

/**
 * gbb_init() - Read in the Google Binary Block (GBB)
 *
 * Allocates space for the GBB and reads in the various pieces
 *
 * @vboot: vboot context
 * @return 0 if OK, -ve on error
 */
static int gbb_init(struct vboot_info *vboot)
{
	struct fmap_entry *entry = &vboot->fmap.readonly.gbb;
	VbCommonParams *cparams = &vboot->cparams;
	u32 offset;
	int ret, i;

	cparams->gbb_size = entry->length;
	cparams->gbb_data = malloc(entry->length);
	if (!cparams->gbb_data)
		return log_msg_ret("buffer", -ENOMEM);

	memset(cparams->gbb_data, 0, cparams->gbb_size);

	offset = entry->offset;

	GoogleBinaryBlockHeader *hdr = cparams->gbb_data;

	ret = gbb_copy_in(vboot, offset, 0, sizeof(GoogleBinaryBlockHeader));
	if (ret)
		return ret;
	log_debug("The GBB signature is at %p and is:", hdr->signature);
	for (i = 0; i < GBB_SIGNATURE_SIZE; i++)
		log_debug(" %02x", hdr->signature[i]);
	log_debug("\n");

	ret = gbb_copy_in(vboot, offset, hdr->hwid_offset, hdr->hwid_size);
	if (ret)
		return ret;

	ret = gbb_copy_in(vboot, offset, hdr->rootkey_offset,
			  hdr->rootkey_size);
	if (ret)
		return ret;

	ret = gbb_copy_in(vboot, offset, hdr->recovery_key_offset,
			  hdr->recovery_key_size);
	if (ret)
		return ret;

	return 0;
}

/**
 * common_params_init() - read in GBB and find vboot's shared data
 *
 * @vboot: vboot context
 * @clear_shared_data: true to clear the shared data region to zeroes
 * @return 0 if OK, -ve on error
 */
static int common_params_init(struct vboot_info *vboot, bool clear_shared_data)
{
	VbCommonParams *cparams = &vboot->cparams;
	int ret;

	memset(cparams, '\0', sizeof(*cparams));

	ret = gbb_init(vboot);
	if (ret)
		return log_msg_ret("gbb", ret);

	cparams->shared_data_blob = vboot->handoff->shared_data;
	cparams->shared_data_size = ARRAY_SIZE(vboot->handoff->shared_data);
	if (clear_shared_data)
		memset(cparams->shared_data_blob, '\0',
		       cparams->shared_data_size);
	log_info("Found shared_data_blob at %lx, size %d\n",
		 (ulong)map_to_sysmem(cparams->shared_data_blob),
		 cparams->shared_data_size);

	return 0;
}

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
 * wipe_unused_memory() - Wipe memory not needed to boot
 *
 * This provides additional security by clearing out memory that might contain
 * things from a previous boot
 *
 * @vboot: vboot context
 * @return 0 if OK, -EPERM if coreboot tables are needed but missing on x86
 *	(fatal error)
 */
static int wipe_unused_memory(struct vboot_info *vboot)
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

	/* Exclude the shared data between bootstub and main firmware */
	memwipe_sub(&wipe, (ulong)vboot->handoff,
		    (ulong)vboot->handoff + sizeof(struct vboot_handoff));

	memwipe_execute(&wipe);

	return 0;
}

static int vboot_do_init_out_flags(struct vboot_info *vboot, u32 out_flags)
{
	if (0 && (out_flags & VB_INIT_OUT_CLEAR_RAM)) {
		if (vboot->disable_memwipe)
			log_warning("Memory wipe requested but not supported\n");
		else
			return wipe_unused_memory(vboot);
	}

	return 0;
}

/**
 * vboot_init_handoff() - Read in the hand-off information from previous phase
 *
 * This handles both booting bare-metal and from coreboot. It collects the
 * provided handoff information and sets things up ready for the read/write
 * phase of vboot, containing the UI.
 *
 * @vboot: vboot context
 */
static int vboot_init_handoff(struct vboot_info *vboot)
{
	struct vboot_handoff *handoff;
	VbSharedDataHeader *vdat;
	int ret;

	if (!vboot_from_cb(vboot)) {
		handoff = bloblist_find(BLOBLISTT_VBOOT_HANDOFF,
					sizeof(*handoff));
		if (!handoff)
			return log_msg_ret("handoff\n", -ENOENT);
	} else {
		handoff = cb_get_vboot_handoff();
	}
	vboot->handoff = handoff;

	/* Set up the common param structure, not clearing shared data */
	ret = common_params_init(vboot, 0);
	if (ret)
		return ret;

	vdat = vboot->cparams.shared_data_blob;
	/*
	 * If the lid is closed, don't count down the boot tries for updates,
	 * since the OS will shut down before it can register success.
	 *
	 * VbInit() was already called in stage A, so we need to update the
	 * vboot internal flags ourself.
	 */
	if (vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN) == 0) {
		/* Tell kernel selection to not count down */
		vdat->flags |= VBSD_NOFAIL_BOOT;
	}

	ret = vboot_do_init_out_flags(vboot, handoff->init_params.out_flags);
	if (ret)
		return log_msg_ret("flags", ret);

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
		blob = bloblist_find(BLOBLISTT_VBOOT_CTX, sizeof(*blob));
		if (!blob)
			return log_msg_ret("blob", -ENOENT);
		vboot->blob = blob;

		/*
		 * This is not actually used since this part of vboot uses the
		 * old v1 API
		 */
		ctx = &blob->ctx;
		vboot->ctx = ctx;
		ctx->non_vboot_context = vboot;
		log_warning("flags %x %d\n", ctx->flags,
			    ((ctx->flags & VB2_CONTEXT_RECOVERY_MODE) != 0));
	} else {
		ret = cb_vboot_rw_init(vboot);
		if (ret)
			return log_msg_ret("cb", ret);
	}

	if (vboot_is_recovery(vboot))
		log_info("Recovery mode\n");
	else
		log_info("Booting from slot %s: vboot->ctx=%p, flags %x\n",
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
	ret = vboot_init_handoff(vboot);
	if (ret)
		return log_msg_ret("handoff", ret);

	return 0;
}
