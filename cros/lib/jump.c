// SPDX-License-Identifier: GPL-2.0
/*
 * Jumping from SPL to U-Boot proper
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <dm.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <os.h>
#include <spi.h>
#include <spl.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>
#include <dm/uclass-internal.h>

/*
 * Enable this to read into RAM. If false, it read directly from FLASH,
 * which only works if flash is memory-mapped, as on x86.
 */
#define USE_RAM		true

int vboot_jump(struct vboot_info *vboot, struct fmap_entry *entry)
{
	struct spl_image_info *spl_image = vboot->spl_image;
	int ret;
#if USE_RAM
	u32 addr;
	char *buf;

	addr = spl_next_phase() == PHASE_SPL ? CONFIG_SPL_TEXT_BASE :
		 CONFIG_SYS_TEXT_BASE;
	buf = map_sysmem(addr, 0);
#else
	ulong mask = CONFIG_ROM_SIZE - 1;
	struct udevice *sf;
	u32 rom_offset;
	u32 addr;
	uint map_size;
	ulong map_base;
	uint offset;

	/* Use the SPI driver to get the memory map */
	ret = uclass_find_first_device(UCLASS_SPI_FLASH, &sf);
	if (ret)
		return log_msg_ret("flash", ret);
	ret = dm_spi_get_mmap(sf, &map_base, &map_size, &offset);
	if (ret)
		return log_msg_ret("mmap", ret);
	rom_offset = (map_base & mask) - CONFIG_ROM_SIZE;
	addr = entry->offset + rom_offset;
#endif

#if USE_RAM
	log_info("Reading firmware offset %x (addr %x, size %x)\n",
		 entry->offset, addr, entry->length);
	/* TODO(sjg@chromium.org): Find out the real end of the buffer */
	ret = fwstore_read_decomp(vboot->fwstore, entry, buf,
				  entry->length * 3);
	if (ret)
		return log_msg_ret("read", ret);
#else
	log_info("Locating firmware offset %x (rom_offset %x, addr %x, size %x)\n",
		 entry->offset, rom_offset, addr, entry->length);
#endif
	log_debug("sp %p, pc %p, spl_image %p\n", &addr, vboot_jump, spl_image);
#ifdef DEBUG
	print_buffer(addr, (void *)addr, 1, 0x20, 0);
#endif
	spl_image->size = entry->length;
	spl_image->entry_point = addr;
	spl_image->load_addr = addr;
	spl_image->os = IH_OS_U_BOOT;
	spl_image->name = "U-Boot";

	return 0;
}
