// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <binman_sym.h>
#include <spl.h>
#include <asm/spl.h>
#include <asm/arch/cpu.h>
#include <asm/arch/fast_spi.h>

#include <dm.h>
#include <spi_flash.h>

/*
 * We need to read well past the end of the region in order for execution from
 * the loaded data to work. It is not clear why.
 */
#define SAFETY_MARGIN	0x4000

binman_sym_declare(ulong, u_boot_spl, image_pos);
binman_sym_declare(ulong, u_boot_spl, size);
/* U-Boot image_pos is declared by common/spl/spl.c */
binman_sym_declare(ulong, u_boot_any, size);

static ulong get_image_pos(void)
{
	return spl_phase() == PHASE_TPL ?
		binman_sym(ulong, u_boot_spl, image_pos) :
		binman_sym(ulong, u_boot_any, image_pos);
}

static ulong get_image_size(void)
{
	return spl_phase() == PHASE_TPL ?
		binman_sym(ulong, u_boot_spl, size) :
		binman_sym(ulong, u_boot_any, size);
}

/* This reads the next phase from mapped SPI flash */
static int rom_load_image(struct spl_image_info *spl_image,
			  struct spl_boot_device *bootdev)
{
	ulong spl_pos = get_image_pos();
	ulong spl_size = get_image_size();
	ulong map_base;
	size_t map_size;
	uint map_offset;
	int ret;

	spl_image->size = CONFIG_SYS_MONITOR_LEN;  /* We don't know SPL size */
	spl_image->entry_point = spl_phase() == PHASE_TPL ?
		CONFIG_SPL_TEXT_BASE : CONFIG_SYS_TEXT_BASE;
	spl_image->load_addr = spl_image->entry_point;
	spl_image->os = IH_OS_U_BOOT;
	spl_image->name = "U-Boot";
	debug("Reading from mapped SPI %lx, size %lx", spl_pos, spl_size);
	ret = fast_spi_get_bios_mmap(&map_base, &map_size, &map_offset);
	if (ret)
		return ret;
	spl_pos += map_base & ~0xff000000;
	debug(", base %lx, pos %lx\n", map_base, spl_pos);
	memcpy((void *)spl_image->load_addr, (void *)spl_pos,
	       spl_size + SAFETY_MARGIN);

	return 0;
}
SPL_LOAD_IMAGE_METHOD("Mapped SPI", 2, BOOT_DEVICE_SPI_MMAP, rom_load_image);

#if CONFIG_IS_ENABLED(SPI_FLASH_SUPPORT)

/* This uses a SPI flash device to read the next phase */
static int spl_fast_spi_load_image(struct spl_image_info *spl_image,
				   struct spl_boot_device *bootdev)
{
	ulong spl_pos = get_image_pos();
	ulong spl_size = get_image_size();
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_SPI_FLASH, &dev);
	if (ret)
		return ret;

	spl_image->size = CONFIG_SYS_MONITOR_LEN;  /* We don't know SPL size */
	spl_image->entry_point = CONFIG_SPL_TEXT_BASE;
	spl_image->load_addr = CONFIG_SPL_TEXT_BASE;
	spl_image->os = IH_OS_U_BOOT;
	spl_image->name = "U-Boot";
	spl_pos &= ~0xff000000;
	debug("Reading from flash %lx, size %lx\n", spl_pos, spl_size);
	ret = spi_flash_read_dm(dev, spl_pos, spl_size + SAFETY_MARGIN,
				(void *)spl_image->load_addr);
	if (ret)
		return ret;

	return 0;
}
SPL_LOAD_IMAGE_METHOD("Fast SPI", 1, BOOT_DEVICE_FAST_SPI,
		      spl_fast_spi_load_image);

void board_boot_order(u32 *spl_boot_list)
{
	bool use_spi_flash = BOOT_FROM_FAST_SPI_FLASH;

	if (use_spi_flash) {
		spl_boot_list[0] = BOOT_DEVICE_FAST_SPI;
		spl_boot_list[1] = BOOT_DEVICE_SPI_MMAP;
	} else {
		spl_boot_list[0] = BOOT_DEVICE_SPI_MMAP;
		spl_boot_list[1] = BOOT_DEVICE_FAST_SPI;
	}
}

#else

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_SPI_MMAP;
}
#endif
