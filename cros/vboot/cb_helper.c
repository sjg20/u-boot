// SPDX-License-Identifier: GPL-2.0+
/*
 * Helper functions used when booting from coreboot
 *
 * Copyright 2021 Google LLC
 */

#include <common.h>
#include <abuf.h>
#include <cbfs.h>
#include <asm/cb_sysinfo.h>
#include <cros/cb_helper.h>
#include <cros/fmap.h>
#include <cros/fwstore.h>
#include <cros/memwipe.h>
#include <cros/vboot.h>

/**
 * fmap_valid() - Check if the FMAP signature is correct
 *
 * @fmap: Pointer to possible FMAP structure
 * @return 0 if OK, -EPERM if FMAP signature is not present
 */
static int fmap_valid(const struct fmap *fmap)
{
	if (memcmp(fmap, FMAP_SIGNATURE, strlen(FMAP_SIGNATURE)) != 0)
		return -EPERM;

	return 0;
}

/**
 * fmap_parse() - parse an FMAP structure to obtain position information
 *
 * This reads a few things from the FMAP to locate useful pieces, including the
 * GBB, main read-only CBFS and the firmware IDs for read-only and read-write
 * A/B.
 *
 * Note that this will only update information that it finds. If the caller
 * needs a particular region, it needs to check if it is present (non-zero
 * position and size).
 *
 * @return 0 if OK, -ve error if the FMAP struct is invalid in some way
 */
static int fmap_parse(const struct fmap *in, struct cros_fmap *fmap)
{
	int ret;
	uint i;

	ret = fmap_valid(in);
	if (ret)
		return log_msg_ret("valid", ret);

	for (i = 0; i < in->nareas; i++) {
		const struct fmap_area *area = &in->areas[i];
		struct fmap_entry *entry = NULL;
		const char *name = (char *)area->name;

		if (!strcmp("GBB", name))
			entry = &fmap->readonly.gbb;
		else if (!strcmp("COREBOOT", name))
			entry = &fmap->readonly.cbfs;
		else if (!strcmp("RO_FRID", name))
			entry = &fmap->readonly.firmware_id;
		else if (!strcmp("RW_FWID_A", name))
			entry = &fmap->readwrite_a.firmware_id;
		else if (!strcmp("RW_FWID_B", name))
			entry = &fmap->readwrite_b.firmware_id;

		if (entry) {
			entry->offset = area->offset;
			entry->length = area->size;
		}
	}

	return 0;
}

int cb_fmap_read(struct vboot_info *vboot)
{
	const struct sysinfo_t *sysinfo = vboot->sysinfo;
	struct fmap_entry entry;
	void *fmap_ptr;
	ulong addr;
	struct abuf buf;
	int ret;

	entry.offset = sysinfo->fmap_offset;
	entry.length = 0x1000;
	log_info("FMAP at %x, length %x\n", entry.offset, entry.length);
	abuf_init(&buf);
	ret = fwstore_entry_mmap(vboot->fwstore, &entry, &addr);
	if (ret) {
		/* Read it into a buffer */
		ret = cros_fwstore_read_entry(vboot->fwstore, &entry, &buf);
		if (ret) {
			abuf_uninit(&buf);
			return log_msg_ret("entry", ret);
		}
		fmap_ptr = abuf_data(&buf);
	} else {
		fmap_ptr = (void *)addr;
	}

	/*
	 * Store the FMAP offset so it can be passed to the kernel in
	 * vboot_update_acpi()
	 */
	vboot->fmap.readonly.fmap = entry;
	ret = fmap_parse(fmap_ptr, &vboot->fmap);
	abuf_uninit(&buf);
	if (ret)
		return log_msg_ret("parse", ret);

	return 0;
}


/**
 * cb_vboot_make_context() - Make a vboot 2 context from sysinfo
 *
 * This parses the handoff information to produce a VB2 context, with the
 * flags field set correctly.
 *
 * @sysinfo: Coreboot sysinfo information
 * @ctxp: Returns a new VB2 context
 */
int cb_vboot_make_context(const struct sysinfo_t *sysinfo,
			  struct vb2_context **ctxp)
{
	const struct vboot_handoff *handoff = sysinfo->vboot_handoff;
	struct vb2_context *ctx;
	u32 out_flags;

	if (!handoff)
		return log_msg_ret("handoff", -ENOENT);
	log_debug("Using vboot_handoff at %p\n", handoff);
	ctx = calloc(sizeof(*ctx), 1);
	if (!ctx)
		return log_msg_ret("ctx", -ENOMEM);

	/* Convert the flags so we don't have to deal with legacy ones */
	out_flags = handoff->init_params.out_flags;
	if (out_flags & VB_INIT_OUT_ENABLE_RECOVERY)
		ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
	if (out_flags & VB_INIT_OUT_ENABLE_DEVELOPER)
		ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	if (handoff->selected_firmware)
		ctx->flags |= VB2_CONTEXT_FW_SLOT_B;

	*ctxp = ctx;

	return 0;
}

enum fmap_compress_t cb_conv_compress_type(uint cbfs_comp_algo)
{
	switch (cbfs_comp_algo) {
	case CBFS_COMPRESS_NONE:
		return FMAP_COMPRESS_NONE;
	case CBFS_COMPRESS_LZMA:
		return FMAP_COMPRESS_LZMA;
	case CBFS_COMPRESS_LZ4:
		return FMAP_COMPRESS_LZ4;
	default:
		return FMAP_COMPRESS_UNKNOWN;
	}
}

int cb_scan_files(struct cbfs_priv *cbfs, struct fmap_section *section)
{
	const struct cbfs_cachenode *node;

	log_debug("Scanning CBFS files\n");
	for (node = cbfs_get_first(cbfs); node; cbfs_get_next(&node)) {
		struct fmap_entry *entry = NULL;

		if (!strcmp("ecrw", node->name)) {
			entry = &section->ec[EC_MAIN].rw;
			entry->cbfs_node = node;
			entry->length = node->data_length;
			entry->unc_length = node->decomp_size;
			entry->compress_algo =
				cb_conv_compress_type(node->comp_algo);
			if (entry->compress_algo == FMAP_COMPRESS_UNKNOWN)
				return log_msg_ret("algo", -EPROTONOSUPPORT);
		} else if (!strcmp("ecrw.hash", node->name)) {
			entry = &section->ec[EC_MAIN].rw;
			entry->cbfs_hash_node = node;
			entry->hash = node->data;
			entry->hash_size = node->data_length;
		}
		if (entry)
			log_debug("- processed %s\n", node->name);
	}

	return 0;
}

int cb_scan_cbfs(struct vboot_info *vboot, uint offset, uint size,
		 struct cbfs_priv **cbfsp)
{
	ulong addr;
	int ret;

	/* Access the CBFS containing our files */
	ret = cros_fwstore_mmap(vboot->fwstore, offset, size, &addr);
	if (ret)
		return log_msg_ret("mmap", ret);
	log_debug("Mapped fstore offset %x, size %x to address %lx\n", offset,
		  size, addr);

	ret = cbfs_init_mem(addr, size, false, cbfsp);
	if (ret) {
		log_err("Invalid CBFS (err=%d)\n", ret);
		return log_msg_ret("cbfs", ret);
	}

	return 0;
}

int cb_setup_unused_memory(struct vboot_info *vboot, struct memwipe *wipe)
{
	const struct sysinfo_t *sysinfo = cb_get_sysinfo();
	int i;

	if (!sysinfo)
		return -EPERM;

	/* Add ranges that describe RAM */
	for (i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->type == CB_MEM_RAM) {
			memwipe_add(wipe, range->base,
				    range->base + range->size);
		}
	}
	/*
	 * Remove ranges that don't. These should take precedence, so they're
	 * done last and in their own loop.
	 */
	for (i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->type != CB_MEM_RAM) {
			memwipe_sub(wipe, range->base,
				    range->base + range->size);
		}
	}

	return 0;
}

const char *cb_read_model(const struct sysinfo_t *sysinfo)
{
	const struct cb_mainboard *mb;
	const char *ptr;

	/* Grab the board name out of the coreboot tables */
	mb = sysinfo->mainboard;
	ptr = (char *)mb->strings + mb->part_number_idx;

	/* Set a maximum length to avoid reading corrupted data */
	if (!mb->strings || strnlen(ptr, 30) >= 30)
		return NULL;

	return (char *)mb->strings + mb->part_number_idx;
}

int cb_vboot_rw_init(struct vboot_info *vboot, struct vb2_context **ctxp)
{
	const struct sysinfo_t *sysinfo = cb_get_sysinfo();
	const char *model;
	int ret;

	if (!sysinfo) {
		log_err("No vboot handoff info\n");
		return -ENOENT;
	}

	/* Grab the board name out of the coreboot tables */
	model = cb_read_model(sysinfo);
	if (model) {
		log_notice("\n");
		log_notice("Starting vboot on %.30s...\n", model);
	}

	ret = cb_vboot_make_context(sysinfo, ctxp);
	if (ret)
		return log_msg_ret("ctx", ret);
	vboot->from_coreboot = true;
	vboot->sysinfo = sysinfo;
	log_debug("Located coreboot sysinfo at %p\n", sysinfo);

	/*
	 * Disable all flag devices except the sysinfo one, since coreboot has
	 */

	return 0;
}

struct vboot_handoff *cb_get_vboot_handoff(void)
{
	return vboot->sysinfo->vboot_handoff;
}

int cb_setup_flashmap(struct vboot_info *vboot)
{
	/*
	 * Read the FMAP, which is the only way to locate things in
	 * the ROM, since U-Boot's devicetree does not contain this
	 * info when booted from coreboot.
	 */
	ret = cb_fmap_read(vboot);
	if (ret)
		return log_msg_ret("fmap", ret);

	ret = cb_scan_cbfs(vboot, vboot->sysinfo->cbfs_offset,
				vboot->sysinfo->cbfs_size, &vboot->cbfs);
	if (ret)
		return log_msg_ret("scan", ret);

	ret = cb_scan_files(vboot->cbfs, section);
	if (ret)
		return log_msg_ret("files", ret);
	if (is_rw) {
		/* Get access to the read-only CBFS, for locale info */
		ret = cb_scan_cbfs(vboot,
					vboot->fmap.readonly.cbfs.offset,
					vboot->fmap.readonly.cbfs.length,
					&vboot->cbfs);
		if (ret)
			return log_msg_ret("ro", ret);
	} else {
		/* We are using the read-only CBFS already */
		vboot->cbfs_ro = vboot->cbfs;
	}

	return 0;
}
