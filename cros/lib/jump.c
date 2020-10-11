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
#include <os.h>
#include <spi.h>
#include <spl.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>
#include <dm/uclass-internal.h>

/*
 * Enable this to read into RAM. If undefined, it read directly from FLASH,
 * which only works if flash is memory-mapped, as on x86.
 */
#define USE_RAM		!IS_ENABLED(CONFIG_X86)

int vboot_jump(struct vboot_info *vboot, struct fmap_entry *entry)
{
	struct spl_image_info *spl_image = vboot->spl_image;
	int ret;
#if USE_RAM
	u32 addr = CONFIG_SYS_TEXT_BASE;
	char *buf = (char *)(ulong)addr;
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
		return log_msg_ret("Cannot get SPI flash", ret);
	ret = dm_spi_get_mmap(sf, &map_base, &map_size, &offset);
	if (ret)
		return log_msg_ret("Could not get flash mmap", ret);
	rom_offset = (map_base & mask) - CONFIG_ROM_SIZE;
	addr = entry->offset + rom_offset;
#endif

#if USE_RAM
	log_info("Reading firmware offset %x (addr %x, size %x)\n",
		 entry->offset, addr, entry->length);
	ret = cros_fwstore_read(vboot->fwstore, entry->offset, entry->length,
				buf);
	if (ret)
		return log_msg_ret("Read fwstore", ret);
#else
	log_info("Locating firmware offset %x (rom_offset %x, addr %x, size %x)\n",
		 entry->offset, rom_offset, addr, entry->length);
	print_buffer(addr, (void *)addr, 1, 0x20, 0);
#endif
	spl_image->size = entry->length;
	spl_image->entry_point = addr;
	spl_image->load_addr = addr;
	spl_image->os = IH_OS_U_BOOT;
	spl_image->name = "U-Boot";

	return 0;
}
