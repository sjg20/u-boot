// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <binman.h>
#include <binman_sym.h>
#include <cbfs.h>
#include <dm.h>
#include <init.h>
#include <spl.h>
#include <spi_flash.h>
#include <asm/arch/gpio.h>
#include <dm/uclass-internal.h>
#include <asm/fsp2/fsp_internal.h>

int arch_cpu_init_dm(void)
{
	struct udevice *dev;
	ofnode node;
	int ret;

	if (spl_phase() != PHASE_BOARD_F)
		return 0;

	/* Probe all GPIO devices to set up the pads */
	ret = uclass_first_device_err(UCLASS_GPIO, &dev);
	if (ret)
		return log_msg_ret("no fsp GPIO", ret);
	node = ofnode_path("fsp");
	if (!ofnode_valid(node))
		return log_msg_ret("no fsp params", -EINVAL);
	ret = hostbridge_config_pads_for_node(dev, node);
	if (ret)
		return log_msg_ret("pad config", ret);

	return ret;
}

#if !defined(CONFIG_TPL_BUILD)
binman_sym_declare(ulong, intel_fsp_m, image_pos);
binman_sym_declare(ulong, intel_fsp_m, size);

static int get_coreboot_fsp(enum fsp_type_t type, ulong map_base,
			    struct binman_entry *entry)
{
	/* Hard-coded position of CBFS in ROM */
	ulong cbfs_base = 0x205000;
	ulong cbfs_size = 0x1bb000;
	struct cbfs_priv *cbfs;
	int ret;

	ret = cbfs_init_mem(map_base + cbfs_base, cbfs_size, &cbfs);
	if (ret)
		return ret;
	if (!ret) {
		const struct cbfs_cachenode *node;

		node = cbfs_find_file(cbfs, "fspm.bin");
		if (!node) {
			printf("no node\n");
			return -ENOENT;
		}

		entry->image_pos = (ulong)node->data;
		entry->size= node->data_length;
	}

	return 0;
}

int fsp_locate_fsp(enum fsp_type_t type, struct binman_entry *entry,
		   bool use_spi_flash, struct udevice **devp,
		   struct fsp_header **hdrp, ulong *rom_offsetp)
{
	ulong mask = CONFIG_ROM_SIZE - 1;
	struct udevice *dev, *sf;
	ulong rom_offset = 0;
	size_t map_size;
	ulong map_base;
	u32 offset;
	int ret;

	/*
	 * Find the devices but don't probe them, since we don't want to
	 * auto-config PCI before silicon init runs
	 */
	ret = uclass_find_first_device(UCLASS_NORTHBRIDGE, &dev);
	if (ret)
		return log_msg_ret("Cannot get northbridge", ret);
	ret = uclass_find_first_device(UCLASS_SPI_FLASH, &sf);
	if (ret)
		return log_msg_ret("Cannot get SPI flash", ret);
	ret = spi_flash_get_mmap(sf, &map_base, &map_size, &offset);
	if (ret)
		return log_msg_ret("Could not get flash mmap", ret);

	if (spl_phase() >= PHASE_BOARD_F) {
		if (type != FSP_S)
			return -EPROTONOSUPPORT;
		ret = binman_entry_find("intel-fsp-s", entry);
		if (ret)
			return log_msg_ret("binman entry", ret);
		if (!use_spi_flash)
			rom_offset = (map_base & mask) - CONFIG_ROM_SIZE;
	} else {
		ret = -ENOENT;
		if (false)
			/* Support using a hybrid image build by coreboot */
			ret = get_coreboot_fsp(type, map_base, entry);
		if (ret) {
			ulong mask = CONFIG_ROM_SIZE - 1;

			if (type != FSP_M)
				return -EPROTONOSUPPORT;
			entry->image_pos = binman_sym(ulong, intel_fsp_m, image_pos);
			entry->size = binman_sym(ulong, intel_fsp_m, size);
			if (entry->image_pos != BINMAN_SYM_MISSING) {
				ret = 0;
				if (use_spi_flash)
					entry->image_pos &= mask;
				else
					entry->image_pos += (map_base & mask);
			} else {
				ret = -ENOENT;
			}
		}
	}
	if (ret)
		return log_msg_ret("Cannot find FSP", ret);
	entry->image_pos += rom_offset;

	/* Use memory-mapped SPI flash by default as it is simpler */
	ret = fsp_get_header(entry->image_pos, entry->size, use_spi_flash,
			     hdrp);
	if (ret)
		return log_msg_ret("fsp_get_header", ret);
	*devp = dev;
	if (rom_offsetp)
		*rom_offsetp = rom_offset;

	return 0;
}
#endif
