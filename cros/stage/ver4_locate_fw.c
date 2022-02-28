// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <ec_commands.h>
#include <misc.h>
#include <os.h>
#include <spi_flash.h>
#include <cros/cros_common.h>
#include <cros/fwstore.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>
#include <vb2_api.h>

#include <vb2_internals_please_do_not_use.h>

/* The max hash size to expect is for SHA512 */
#define VBOOT_MAX_HASH_SIZE	VB2_SHA512_DIGEST_SIZE

/* Buffer size for reading from firmware */
#define TODO_BLOCK_SIZE 1024

/**
 * vboot_save_hash() - save a hash to a secure location
 *
 * This can be verified in the resume path.
 *
 * @digest: Hash to save
 * @digest_size: Size of hash to save
 * @return 0 on success, -ve on error.
 */
static int vboot_save_hash(void *digest, size_t digest_size)
{
	int ret;

	/* Ensure the digests being saved match the EC's slot size */
	assert(digest_size == EC_VSTORE_SLOT_SIZE);

	ret = cros_nvdata_write_walk(CROS_NV_VSTORE, digest, digest_size);
	if (ret)
		return log_msg_ret("write", ret);

	/* Assert the slot is locked on successful write */
	ret = cros_nvdata_lock_walk(CROS_NV_VSTORE);
	if (ret)
		return log_msg_ret("lock", ret);

	return 0;
}

/**
 * vboot_retrieve_hash() - get a previously saved hash digest
 *
 * @digest: Place to put hash
 * @digest_size: Expected size of hash to read
 * @return 0 on success, -ve on error.
 */
static int vboot_retrieve_hash(void *digest, size_t digest_size)
{
	/* Ensure the digests being saved match the EC's slot size */
	assert(digest_size == EC_VSTORE_SLOT_SIZE);

	return cros_nvdata_read_walk(CROS_NV_VSTORE, digest, digest_size);
}

/**
 * handle_digest_result() - Take action based on the calculated hash
 *
 * If we don't need to verify the resume path, or cannot, then there is nothing
 * to do.
 *
 * If resuming, we check the hash and fail if there is a mismatch
 * It booting for thefirst time, save the hash so it can be used in resume
 *
 * @vboot: vboot info
 * @slot_hash: Hash digest
 * @slot_hash_sz: Number of bytes in ash
 * @return 0 if OK, -ve on error
 */
static int handle_digest_result(struct vboot_info *vboot, void *slot_hash,
				size_t slot_hash_sz)
{
	int is_resume;
	int ret;

	/*
	 * Chrome EC is the only support for vboot_save_hash() &
	 * vboot_retrieve_hash(), if Chrome EC is not enabled then return.
	 */
	if (!IS_ENABLED(CONFIG_CROS_EC)) {
		log_info("No Chrome OS EC\n");
		return 0;
	}

	/*
	 * Nothing to do since resuming on the platform doesn't require
	 * vboot verification again.
	 */
	if (!vboot->resume_path_same_as_boot) {
		log_info("Resume does not require verification\n");
		return 0;
	}

	/*
	 * If RW memory init code is not used, then we don't need to worry
	 * about hashing
	 */
	if (vboot->meminit_in_ro) {
		log_info("Memory init is in read-only flash\n");
		return 0;
	}

	is_resume = vboot_platform_is_resuming();
	log_info("is_resume=%d\n", is_resume);
	if (is_resume > 0) {
		u8 saved_hash[VBOOT_MAX_HASH_SIZE];
		const size_t saved_hash_sz = sizeof(saved_hash);
		int ret;

		assert(slot_hash_sz == saved_hash_sz);

		log_debug("Platform is resuming\n");

		ret = vboot_retrieve_hash(saved_hash, saved_hash_sz);
		if (ret) {
			log_err("Couldn't retrieve saved hash\n");
			return ret;
		}

		if (memcmp(saved_hash, slot_hash, slot_hash_sz)) {
			log_err("Hash mismatch on resume\n");
			return ret;
		}
	} else if (is_resume < 0) {
		log_err("Unable to determine if platform resuming (%d)",
			is_resume);
	}

	log_debug("Saving vboot hash\n");

	/* Always save the hash for the current boot */
	ret = vboot_save_hash(slot_hash, slot_hash_sz);
	if (ret) {
		log_err("Error %d saving vboot hash\n", ret);
		/*
		 * Though this is an error, don't report it up since it could
		 * lead to a reboot loop. The consequence of this is that
		 * we will most likely fail resuming because of EC issues or
		 * the hash digest not matching.
		 */
		return 0;
	}

	return 0;
}

/**
 * hash_body() - hash the firmware body and decide if it is valid
 *
 * This uses handle_digest_result() to either store the hash (for first boot),
 * or decide whether the hash is valid (for resume).
 *
 * @vboot: vboot info
 * @fw_main: fwstore device containing the firmware
 * @return 0 if OK, -ve on error
 */
static int hash_body(struct vboot_info *vboot, struct udevice *fw_main)
{
	const size_t hash_digest_sz = VBOOT_MAX_HASH_SIZE;
	u8 hash_digest[VBOOT_MAX_HASH_SIZE];
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	u8 block[TODO_BLOCK_SIZE];
	uint expected_size;
	int ret, blk;

	/*
	 * Clear the full digest so that any hash digests less than the
	 * max size have trailing zeros
	 */
	memset(hash_digest, 0, hash_digest_sz);

	bootstage_mark(BOOTSTAGE_VBOOT_START_HASH_BODY);

	expected_size = fwstore_reader_size(fw_main);
	log_info("Hashing firmware body, expected size %x\n", expected_size);

	/* Start the body hash */
	ret = vb2api_init_hash(ctx, VB2_HASH_TAG_FW_BODY);
	if (ret)
		return log_msg_retz("init hash", ret);

	/*
	 * Honor vboot's RW slot size. The expected size is pulled out of
	 * the preamble and obtained through vb2api_init_hash() above. By
	 * creating sub region the RW slot portion of the boot media is
	 * limited.
	 */
	ret = fwstore_reader_restrict(fw_main, 0, expected_size);
	if (ret) {
		log_err("Unable to restrict firmware size\n");
		return log_msg_ret("restrict", ret);
	}

	/* Extend over the body */
	for (blk = 0; ; blk++) {
		int nbytes;

		bootstage_start(BOOTSTAGE_ACCUM_VBOOT_FIRMWARE_READ, NULL);
		nbytes = misc_read(fw_main, -1, block, TODO_BLOCK_SIZE);
#ifdef DEBUG
		print_buffer(blk * TODO_BLOCK_SIZE, block, 1,
			     nbytes > 0x20 ? 0x20 : nbytes, 0);
#endif
		bootstage_accum(BOOTSTAGE_ACCUM_VBOOT_FIRMWARE_READ);
		if (nbytes < 0)
			return log_msg_ret("Read fwstore", nbytes);
		else if (!nbytes)
			break;

		ret = vb2api_extend_hash(ctx, block, nbytes);
		if (ret)
			return log_msg_retz("extend hash", ret);
	}
	bootstage_mark(BOOTSTAGE_VBOOT_DONE_HASHING);

	/* Check the result (with RSA signature verification) */
	ret = vb2api_check_hash_get_digest(ctx, hash_digest, hash_digest_sz);
	if (ret)
		return log_msg_retz("check hash", ret);

	bootstage_mark(BOOTSTAGE_VBOOT_END_HASH_BODY);

	if (handle_digest_result(vboot, hash_digest, hash_digest_sz))
		return log_msg_retz("handle result", ret);
	vboot->fw_size = expected_size;

	return VB2_SUCCESS;
}

int vboot_ver4_locate_fw(struct vboot_info *vboot)
{
	struct fmap_entry *entry;
	struct udevice *dev;
	int ret;

	if (vboot_is_slot_a(vboot))
		entry = &vboot->fmap.readwrite_a.spl;
	else
		entry = &vboot->fmap.readwrite_b.spl;
	log_info("Setting up firmware reader at %x, size %x\n", entry->offset,
		 entry->length);
	ret = fwstore_get_reader_dev(vboot->fwstore, entry->offset,
				     entry->length, &dev);
	/* TODO(sjg@chromium.org): Perhaps this should be fatal? */
	if (ret)
		return log_msg_ret("Cannot get reader device", ret);

	ret = hash_body(vboot, dev);
	if (ret) {
		log_info("Reboot requested (%x)\n", ret);
		return VB2_REQUEST_REBOOT;
	}

	return 0;
}
