// SPDX-License-Identifier: GPL-2.0
/*
 * Verified Boot for Embedded (VBE) common functions
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <bootstage.h>
#include <dm.h>
#include <spl.h>
#include <mapmem.h>
#include <memalign.h>
#include <linux/types.h>
#include "vbe_common.h"

ulong h_vbe_load_read(struct spl_load_info *load, ulong off, ulong size,
		      void *buf)
{
	struct blk_desc *desc = load->priv;
	lbaint_t sector = off >> desc->log2blksz;
	lbaint_t count = size >> desc->log2blksz;
	int ret;

	log_debug("vbe read log2blksz %x offset %lx sector %lx count %lx\n",
		  desc->log2blksz, (ulong)off, (long)sector, (ulong)count);
// 	print_buffer(0x000631f0, (void *)0x000631f0, 1, 0x200, 0);

	ret = blk_dread(desc, sector, count, buf) << desc->log2blksz;
	log_debug("ret=%d\n", ret);
	return ret;
}

int vbe_read_fit(struct udevice *blk, ulong area_offset, ulong area_size,
		 struct spl_image_info *image, ulong *load_addrp, char **namep)
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

ofnode vbe_get_node(void)
{
	return ofnode_path("/bootstd/firmware0");
}
