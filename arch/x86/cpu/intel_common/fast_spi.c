// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <asm/io.h>
#include <asm/pci.h>
#include <asm/arch/fast_spi.h>
#include <asm/arch/iomap.h>

/*
 * Returns bios_start and fills in size of the BIOS region.
 */
ulong fast_spi_get_bios_region(struct fast_spi_regs *regs, size_t *bios_size)
{
	ulong bios_start, bios_end;

	/*
	 * BIOS_BFPREG provides info about BIOS-Flash Primary Region Base and
	 * Limit. Base and Limit fields are in units of 4K.
	 */
	u32 val = readl(&regs->bfp);

	bios_start = (val & SPIBAR_BFPREG_PRB_MASK) << 12;
	bios_end = (((val & SPIBAR_BFPREG_PRL_MASK) >>
		     SPIBAR_BFPREG_PRL_SHIFT) + 1) << 12;
	*bios_size = bios_end - bios_start;

	return bios_start;
}

int fast_spi_get_bios_mmap(ulong *map_basep, size_t *map_sizep, uint *offsetp)
{
	struct fast_spi_regs *regs;
	ulong bar, base, mmio_base;

	/* Special case to find mapping without probing the device */
	pci_x86_read_config(PCH_DEV_SPI, PCI_BASE_ADDRESS_0, &bar, PCI_SIZE_32);
	mmio_base = bar & PCI_BASE_ADDRESS_MEM_MASK;
	regs = (struct fast_spi_regs *)mmio_base;
	base = fast_spi_get_bios_region(regs, map_sizep);
	*map_basep = (u32)-*map_sizep - base;
	*offsetp = base;

	return 0;
}
