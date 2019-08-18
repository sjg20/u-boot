/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SOC_INTEL_COMMON_BLOCK_FAST_SPI_DEF_H
#define SOC_INTEL_COMMON_BLOCK_FAST_SPI_DEF_H

/* PCI configuration registers */

#define SPIDVID_OFFSET			0x0
#define SPIBAR_BIOS_CONTROL		0xdc

/* Bit definitions for BIOS_CONTROL */
#define SPIBAR_BIOS_CONTROL_WPD			BIT(0)
#define SPIBAR_BIOS_CONTROL_LOCK_ENABLE		BIT(1)
#define SPIBAR_BIOS_CONTROL_CACHE_DISABLE	BIT(2)
#define SPIBAR_BIOS_CONTROL_PREFETCH_ENABLE	BIT(3)
#define SPIBAR_BIOS_CONTROL_EISS		BIT(5)
#define SPIBAR_BIOS_CONTROL_BILD		BIT(7)

/* Register offsets from the MMIO region base (PCI_BASE_ADDRESS_0) */
struct fast_spi_regs {
	u32 bfp;
	u32 hsfsts_ctl;
	u32 faddr;
	u32 dlock;

	u32 fdata[0x10];

	u8 spare[0x84 - 0x50];
	u32 fpr[8];
	u16 preop;
	u16 optype;
	u32 opmenu_lower;
	u32 opmenu_upper;

	u32 space2;
	u32 fdoc;
	u32 fdod;
	u32 spare3[4];
	u32 ptinx;
	u32 ptdata;
};
check_member(fast_spi_regs, ptdata, 0xd0);

/* Bit definitions for BFPREG (0x00) register */
#define SPIBAR_BFPREG_PRB_MASK		0x7fff
#define SPIBAR_BFPREG_PRL_SHIFT		16
#define SPIBAR_BFPREG_PRL_MASK		(0x7fff << SPIBAR_BFPREG_PRL_SHIFT)
#define SPIBAR_BFPREG_SBRS		BIT(31)

/* Bit definitions for HSFSTS_CTL (0x04) register */
#define SPIBAR_HSFSTS_FDBC_MASK	(0x3f << 24)
#define SPIBAR_HSFSTS_FDBC(n)		(((n) << 24) & SPIBAR_HSFSTS_FDBC_MASK)
#define SPIBAR_HSFSTS_WET		BIT(21)
#define SPIBAR_HSFSTS_FCYCLE_MASK	(0xf << 17)
#define SPIBAR_HSFSTS_FCYCLE(cyc)	(((cyc) << 17) \
					& SPIBAR_HSFSTS_FCYCLE_MASK)
/* Supported flash cycle types */
#define SPIBAR_HSFSTS_CYCLE_READ	SPIBAR_HSFSTS_FCYCLE(0)
#define SPIBAR_HSFSTS_CYCLE_WRITE	SPIBAR_HSFSTS_FCYCLE(2)
#define SPIBAR_HSFSTS_CYCLE_4K_ERASE	SPIBAR_HSFSTS_FCYCLE(3)
#define SPIBAR_HSFSTS_CYCLE_64K_ERASE	SPIBAR_HSFSTS_FCYCLE(4)
#define SPIBAR_HSFSTS_CYCLE_RD_STATUS	SPIBAR_HSFSTS_FCYCLE(8)

#define SPIBAR_HSFSTS_FGO		BIT(16)
#define SPIBAR_HSFSTS_FLOCKDN		BIT(15)
#define SPIBAR_HSFSTS_FDV		BIT(14)
#define SPIBAR_HSFSTS_FDOPSS		BIT(13)
#define SPIBAR_HSFSTS_WRSDIS		BIT(11)
#define SPIBAR_HSFSTS_SAF_CE		BIT(8)
#define SPIBAR_HSFSTS_SAF_ACTIVE	BIT(7)
#define SPIBAR_HSFSTS_SAF_LE		BIT(6)
#define SPIBAR_HSFSTS_SCIP		BIT(5)
#define SPIBAR_HSFSTS_SAF_DLE		BIT(4)
#define SPIBAR_HSFSTS_SAF_ERROR		BIT(3)
#define SPIBAR_HSFSTS_AEL		BIT(2)
#define SPIBAR_HSFSTS_FCERR		BIT(1)
#define SPIBAR_HSFSTS_FDONE		BIT(0)
#define SPIBAR_HSFSTS_W1C_BITS		0xff

#define WPSR_MASK_SRP0_BIT 0x80

/* Bit definitions for FADDR (0x08) register */
#define SPIBAR_FADDR_MASK		0x7FFFFFF

/* Bit definitions for DLOCK (0x0C) register */
#define SPIBAR_DLOCK_PR0LOCKDN		BIT(8)
#define SPIBAR_DLOCK_PR1LOCKDN		BIT(9)
#define SPIBAR_DLOCK_PR2LOCKDN		BIT(10)
#define SPIBAR_DLOCK_PR3LOCKDN		BIT(11)
#define SPIBAR_DLOCK_PR4LOCKDN		BIT(12)

/* Maximum bytes of data that can fit in FDATAn (0x10) registers */
#define SPIBAR_FDATA_FIFO_SIZE		0x40

/* Bit definitions for FDOC (0xB4) register */
#define SPIBAR_FDOC_COMPONENT		BIT(12)
#define SPIBAR_FDOC_FDSI_1		BIT(2)

/* Flash Descriptor Component Section - Component 0 Density Bit Settings */
#define FLCOMP_C0DEN_MASK		0xF
#define FLCOMP_C0DEN_8MB		4
#define FLCOMP_C0DEN_16MB		5
#define FLCOMP_C0DEN_32MB		6

/* Bit definitions for FPRn (0x84 + (4 * n)) registers */
#define SPIBAR_FPR_WPE			BIT(31) /* Flash Write protected */
#define SPIBAR_FPR_MAX			5

/* Programmable values for OPMENU_LOWER(0xA8) & OPMENU_UPPER(0xAC) register */
#define SPI_OPMENU_0			0x01 /* WRSR: Write Status Register */
#define SPI_OPTYPE_0			0x01 /* Write, no address */
#define SPI_OPMENU_1			0x02 /* BYPR: Byte Program */
#define SPI_OPTYPE_1			0x03 /* Write, address required */
#define SPI_OPMENU_2			0x03 /* READ: Read Data */
#define SPI_OPTYPE_2			0x02 /* Read, address required */
#define SPI_OPMENU_3			0x05 /* RDSR: Read Status Register */
#define SPI_OPTYPE_3			0x00 /* Read, no address */
#define SPI_OPMENU_4			0x20 /* SE20: Sector Erase 0x20 */
#define SPI_OPTYPE_4			0x03 /* Write, address required */
#define SPI_OPMENU_5			0x9f /* RDID: Read ID */
#define SPI_OPTYPE_5			0x00 /* Read, no address */
#define SPI_OPMENU_6			0xd8 /* BED8: Block Erase 0xd8 */
#define SPI_OPTYPE_6			0x03 /* Write, address required */
#define SPI_OPMENU_7			0x0b /* FAST: Fast Read */
#define SPI_OPTYPE_7			0x02 /* Read, address required */
#define SPI_OPMENU_UPPER ((SPI_OPMENU_7 << 24) | (SPI_OPMENU_6 << 16) | \
			  (SPI_OPMENU_5 << 8) | SPI_OPMENU_4)
#define SPI_OPMENU_LOWER ((SPI_OPMENU_3 << 24) | (SPI_OPMENU_2 << 16) | \
			  (SPI_OPMENU_1 << 8) | SPI_OPMENU_0)
#define SPI_OPTYPE ((SPI_OPTYPE_7 << 14) | (SPI_OPTYPE_6 << 12) | \
		    (SPI_OPTYPE_5 << 10) | (SPI_OPTYPE_4 << 8)  | \
		    (SPI_OPTYPE_3 << 6) | (SPI_OPTYPE_2 << 4)   | \
		    (SPI_OPTYPE_1 << 2) | (SPI_OPTYPE_0))
#define SPI_OPPREFIX ((0x50 << 8) | 0x06) /* EWSR and WREN */

/* Bit definitions for PTINX (0xCC) register */
#define SPIBAR_PTINX_COMP_0		(0 << 14)
#define SPIBAR_PTINX_COMP_1		(1 << 14)
#define SPIBAR_PTINX_HORD_SFDP		(0 << 12)
#define SPIBAR_PTINX_HORD_PARAM		(1 << 12)
#define SPIBAR_PTINX_HORD_JEDEC		(2 << 12)
#define SPIBAR_PTINX_IDX_MASK		0xffc

/* Register Offsets of BIOS Flash Program Registers */
#define SPIBAR_RESET_LOCK               0xF0
#define SPIBAR_RESET_CTRL               0xF4
#define SPIBAR_RESET_DATA               0xF8

/* Programmable values of Bit0 (SSL) of Set STRAP MSG LOCK (0xF0) Register */
#define SPIBAR_RESET_LOCK_DISABLE	0 /* Set_Strap Lock(SSL) Bit 0 = 0 */
#define SPIBAR_RESET_LOCK_ENABLE	1 /* Set_Strap Lock(SSL) Bit 0 = 1 */

/* Programmable values of Bit0(SSMS) of Set STRAP MSG Control (0xF4) Register*/
#define SPIBAR_RESET_CTRL_SSMC		1 /* Set_Strap Mux Select(SSMS) Bit=1*/

#define SPIBAR_HWSEQ_XFER_TIMEOUT_MS	5000 /* max 5s*/

ulong fast_spi_get_bios_region(struct fast_spi_regs *regs, size_t *bios_size);

int fast_spi_get_bios_mmap(ulong *map_basep, size_t *map_sizep, uint *offsetp);

#endif	/* SOC_INTEL_COMMON_BLOCK_FAST_SPI_DEF_H */
