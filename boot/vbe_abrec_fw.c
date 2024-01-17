// SPDX-License-Identifier: GPL-2.0
/*
 * Verified Boot for Embedded (VBE) loading firmware phases
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY LOGC_BOOT

#include <common.h>
#include <binman_sym.h>
#include <bloblist.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <bootstage.h>
#include <display_options.h>
#include <dm.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <memalign.h>
#include <mmc.h>
#include <spl.h>
#include <vbe.h>
#include <dm/device-internal.h>
#include "vbe_abrec.h"
#include "vbe_common.h"

#define USE_BOOTMETH	false

binman_sym_declare(ulong, vbe_a, image_pos);
binman_sym_declare(ulong, vbe_b, image_pos);
binman_sym_declare(ulong, vbe_recovery, image_pos);

binman_sym_declare(ulong, vbe_a, size);
binman_sym_declare(ulong, vbe_b, size);
binman_sym_declare(ulong, vbe_recovery, size);

static int vbe_read_fit(struct udevice *blk, ulong area_offset,
			ulong area_size, struct spl_image_info *image,
			ulong *load_addrp, char **namep)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, sbuf, MMC_MAX_BLOCK_LEN);
	const char *fit_uname, *fit_uname_config;
	struct bootm_headers images = {};
	ulong size, blknum, addr, len, load_addr, num_blks, spl_load_addr;
	ulong aligned_size;
	enum image_phase_t phase;
	struct blk_desc *desc;
	int node, ret;
	bool for_spl;
	void *buf;

	log_debug("blk=%s\n", blk->name);
	desc = dev_get_uclass_plat(blk);

	/* read in one block to find the FIT size */
	blknum = area_offset / desc->blksz;
	log_debug("read at %lx, blknum %lx\n", area_offset, blknum);
	ret = blk_read(blk, blknum, 1, sbuf);
	if (ret < 0)
		return log_msg_ret("rd", ret);

	ret = fdt_check_header(sbuf);
	if (ret < 0)
		return log_msg_ret("fdt", -EINVAL);
	size = fdt_totalsize(sbuf);
	if (size > area_size)
		return log_msg_ret("fdt", -E2BIG);
	log_debug("FIT size %lx\n", size);
	aligned_size = ALIGN(size, desc->blksz);

	/*
	 * Load the FIT into the SPL memory. This is typically a FIT with
	 * external data, so this is quite small, perhaps a few KB.
	 */
	buf = malloc(aligned_size);
	addr = map_to_sysmem(buf);
	num_blks = aligned_size / desc->blksz;
	log_debug("read %lx, %lx blocks to %lx / %p\n", aligned_size, num_blks,
		  addr, buf);
	ret = blk_read(blk, blknum, num_blks, buf);
	if (ret < 0)
		return log_msg_ret("rd", ret);

	/* figure out the phase to load */
	phase = IS_ENABLED(CONFIG_VPL_BUILD) ? IH_PHASE_SPL : IH_PHASE_U_BOOT;

	/*
	 * Load the image from the FIT. We ignore any load-address information
	 * so in practice this simply locates the image in the external-data
	 * region and returns its address and size. Since we only loaded the FIT
	 * itself, only a part of the image will be present, at best.
	 */
	fit_uname = NULL;
	fit_uname_config = NULL;
	log_debug("loading FIT\n");

	if (spl_phase() == PHASE_SPL) {
		struct spl_load_info info;
// 		ulong ext_data_offset;

// 		ext_data_offset = ALIGN(area_offset + size, 4);
// 		log_debug("doing SPL with area_offset %lx + fdt_size %lx = ext_data_offset %lx\n",
// 			  area_offset, ALIGN(size, 4), ext_data_offset);

		spl_load_init(&info, h_vbe_load_read, desc, desc->blksz);
		spl_set_phase(&info, IH_PHASE_U_BOOT);
		log_debug("doing SPL from %s blksz %lx log2blksz %x area_offset %lx + fdt_size %lx\n",
			  blk->name, desc->blksz, desc->log2blksz, area_offset, ALIGN(size, 4));
// 		spl_set_ext_data_offset(&info, ext_data_offset);
		ret = spl_load_simple_fit(image, &info, area_offset, buf);
		log_debug("spl_load_abrec_fit() ret=%d\n", ret);

		return ret;
	}

	ret = fit_image_load(&images, addr, &fit_uname, &fit_uname_config,
			     IH_ARCH_DEFAULT, image_ph(phase, IH_TYPE_FIRMWARE),
			     BOOTSTAGE_ID_FIT_SPL_START, FIT_LOAD_IGNORED,
			     &load_addr, &len);
	if (ret == -ENOENT) {
		ret = fit_image_load(&images, addr, &fit_uname,
				     &fit_uname_config, IH_ARCH_DEFAULT,
				     image_ph(phase, IH_TYPE_LOADABLE),
				     BOOTSTAGE_ID_FIT_SPL_START,
				     FIT_LOAD_IGNORED, &load_addr, &len);
	}
	if (ret < 0)
		return log_msg_ret("ld", ret);
	node = ret;
	log_debug("loaded to %lx\n", load_addr);

	for_spl = !USE_BOOTMETH && CONFIG_IS_ENABLED(RELOC_LOADER);
	if (for_spl) {
		image->size = len;
		ret = spl_reloc_prepare(image, &spl_load_addr);
		if (ret)
			return log_msg_ret("spl", ret);
	}

	/* For FIT external data, read in the external data */
	log_debug("load_addr %lx len %lx addr %lx aligned_size %lx\n",
		  load_addr, len, addr, aligned_size);
	if (load_addr + len > addr + aligned_size) {
		ulong base, full_size, offset, extra;
		void *base_buf;

		/* Find the start address to load from */
		base = ALIGN_DOWN(load_addr, desc->blksz);

		offset = area_offset + load_addr - addr;
		blknum = offset / desc->blksz;
		extra = offset % desc->blksz;

		/*
		 * Get the total number of bytes to load, taking care of
		 * block alignment
		 */
		full_size = len + extra;

		/*
		 * Get the start block number, number of blocks and the address
		 * to load to, then load the blocks
		 */
		num_blks = DIV_ROUND_UP(full_size, desc->blksz);
		if (for_spl)
			base = spl_load_addr;
		base_buf = map_sysmem(base, full_size);
		ret = blk_read(blk, blknum, num_blks, base_buf);
		log_debug("read offset %lx blknum %lx full_size %lx num_blks %lx to %lx / %p: ret=%d\n",
			  offset, blknum, full_size, num_blks, base, base_buf,
			  ret);
		if (ret < 0)
			return log_msg_ret("rd", ret);
		if (extra) {
			log_debug("move %p %p %lx\n", base_buf,
				  base_buf + extra, len);
			memmove(base_buf, base_buf + extra, len);
		}
// 		print_buffer(0, base_buf, 1, 0x10, 0);
// 		uint from = ALIGN_DOWN(len - 0x40, 0x10);
// 		print_buffer(from, base_buf + from, 1, 0x50, 0);
// 		print_buffer(0, base_buf, 1, len, 0);
	}
	if (load_addrp)
		*load_addrp = load_addr;
	if (namep) {
		*namep = strdup(fdt_get_name(buf, node, NULL));
		if (!namep)
			return log_msg_ret("nam", -ENOMEM);
	}

	return 0;
}

/**
 * vbe_abrec_read_bootflow_fw() - Create a bootflow for firmware
 *
 * Locates and loads the firmware image (FIT) needed for the next phase. The FIT
 * should ideally use external data, to reduce the amount of it that needs to be
 * read.
 *
 * @bdev: bootdev device containing the firmwre
 * @meth: VBE abrec bootmeth
 * @blow: Place to put the created bootflow, on success
 * @return 0 if OK, -ve on error
 */
int vbe_abrec_read_bootflow_fw(struct udevice *dev, struct bootflow *bflow)
{
	struct udevice *media = dev_get_parent(bflow->dev);
	struct udevice *meth = bflow->method;
	struct abrec_priv *priv = dev_get_priv(meth);
	ulong len, load_addr;
	struct udevice *blk;
	int ret;

	log_debug("media=%s\n", media->name);
	ret = blk_get_from_parent(media, &blk);
	if (ret)
		return log_msg_ret("med", ret);

	ret = vbe_read_fit(blk, priv->area_start + priv->skip_offset,
			   priv->area_size, NULL, &load_addr, &bflow->name);
	if (ret)
		return log_msg_ret("vbe", ret);

	/* set up the bootflow with the info we obtained */
	bflow->blk = blk;
	bflow->buf = map_sysmem(load_addr, len);
	bflow->size = len;

	return 0;
}

static int abrec_load_from_image(struct spl_image_info *image,
				  struct spl_boot_device *bootdev)
{
	struct vbe_handoff *handoff;
	int ret;

	log_debug("here\n");
	if (spl_phase() != PHASE_VPL && spl_phase() != PHASE_SPL)
		return -ENOENT;

	ret = bloblist_ensure_size(BLOBLISTT_VBE, sizeof(struct vbe_handoff),
				   0, (void **)&handoff);
	if (ret)
		return log_msg_ret("ro", ret);

	if (USE_BOOTMETH) {
		struct udevice *meth, *bdev;
		struct abrec_priv *priv;
		struct bootflow bflow;

		vbe_find_first_device(&meth);
		if (!meth)
			return log_msg_ret("vd", -ENODEV);
		log_debug("vbe dev %s\n", meth->name);
		ret = device_probe(meth);
		if (ret)
			return log_msg_ret("probe", ret);

		priv = dev_get_priv(meth);
		log_debug("abrec %s\n", priv->storage);
		ret = bootdev_find_by_label(priv->storage, &bdev, NULL);
		if (ret)
			return log_msg_ret("bd", ret);
		log_debug("bootdev %s\n", bdev->name);

		bootflow_init(&bflow, bdev, meth);
		ret = bootmeth_read_bootflow(meth, &bflow);
		log_debug("\nfw ret=%d\n", ret);
		if (ret)
			return log_msg_ret("rd", ret);

		/* jump to the image */
		image->flags = SPL_SANDBOXF_ARG_IS_BUF;
		image->arg = bflow.buf;
		image->size = bflow.size;
		log_debug("Image: %s at %p size %x\n", bflow.name, bflow.buf,
			  bflow.size);

		/* this is not used from now on, so free it */
		bootflow_free(&bflow);
	} else {
		struct udevice *media, *blk;
		ulong offset, size;

		ret = uclass_get_device_by_seq(UCLASS_MMC, 1, &media);
		if (ret)
			return log_msg_ret("vdv", ret);
		ret = blk_get_from_parent(media, &blk);
		if (ret)
			return log_msg_ret("med", ret);
		offset = binman_sym(ulong, vbe_a, image_pos);
		size = binman_sym(ulong, vbe_a, size);
		log_debug("offset=%lx size=%lx\n", offset, size);

		ret = vbe_read_fit(blk, offset, size, image, NULL, NULL);
		if (ret)
			return log_msg_ret("vbe", ret);
		if (spl_phase() == PHASE_VPL) {
			image->load_addr = spl_get_image_text_base();
			image->entry_point = image->load_addr;
		}
	}

	/* Record that VBE was used in this phase */
	handoff->phases |= 1 << spl_phase();

	return 0;
}
SPL_LOAD_IMAGE_METHOD("vbe_abrec", 5, BOOT_DEVICE_VBE,
		      abrec_load_from_image);
