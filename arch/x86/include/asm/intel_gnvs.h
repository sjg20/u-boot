/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation.
 *
 * Taken from coreboot intelblocks/nvs.h
 * Copyright 2019 Google LLC
 */

#ifndef _INTEL_GNVS_H_
#define _INTEL_GNVS_H_

/*
 * chromeos_acpi_t portion of ACPI GNVS is assumed to live at
 * 0x100 - 0x1000.  When defining global_nvs_t, use check_member
 * to ensure that it is properly aligned:
 *
 *   check_member(global_nvs_t, chromeos, GNVS_CHROMEOS_ACPI_OFFSET);
 */
#define GNVS_CHROMEOS_ACPI_OFFSET 0x100

struct __packed chromeos_acpi {
	/* ChromeOS specific */
	u32	vbt0;		// 00 boot reason
	u32	vbt1;		// 04 active main firmware
	u32	vbt2;		// 08 active ec firmware
	u16	vbt3;		// 0c CHSW
	u8	vbt4[256];	// 0e HWID
	u8	vbt5[64];	// 10e FWID
	u8	vbt6[64];	// 14e FRID - 275
	u32	vbt7;		// 18e active main firmware type
	u32	vbt8;		// 192 recovery reason
	u32	vbt9;		// 196 fmap base address
	u8	vdat[3072];	// 19a VDAT space filled by verified boot
	u32	vbt10;		// d9a smbios bios version
	u32	mehh[8];	// d9e management engine hash
	u32	ramoops_base;	// dbe ramoops base address
	u32	ramoops_len;	// dc2 ramoops length
	u32	vpd_ro_base;	// dc6 pointer to RO_VPD
	u32	vpd_ro_size;	// dca size of RO_VPD
	u32	vpd_rw_base;	// dce pointer to RW_VPD
	u32	vpd_rw_size;	// dd2 size of RW_VPD
	u8	pad[298];	// dd6-eff
};

struct __packed acpi_global_nvs {
	/* Miscellaneous */
	u8	pcnt; /* 0x00 - Processor Count */
	u8	ppcm; /* 0x01 - Max PPC State */
	u8	lids; /* 0x02 - LID State */
	u8	pwrs; /* 0x03 - AC Power State */
	u8	dpte; /* 0x04 - Enable DPTF */
	u32	cbmc; /* 0x05 - 0x08 - coreboot Memory Console */
	u64	pm1i; /* 0x09 - 0x10 - System Wake Source - PM1 Index */
	u64	gpei; /* 0x11 - 0x18 - GPE Wake Source */
	u64	nhla; /* 0x19 - 0x20 - NHLT Address */
	u32	nhll; /* 0x21 - 0x24 - NHLT Length */
	u32	prt0; /* 0x25 - 0x28 - PERST_0 Address */
	u8	scdp; /* 0x29 - SD_CD GPIO portid */
	u8	scdo; /* 0x2a - GPIO pad offset relative to the community */
	u8	uior; /* 0x2b - UART debug controller init on S3 resume */
	u8	ecps; /* 0x2c - SGX Enabled status */
	u64	emna; /* 0x2d - 0x34 EPC base address */
	u64	elng; /* 0x35 - 0x3C EPC Length */
	u8	unused1[0x100 - 0x3d];		/* Pad out to 256 bytes */
#ifdef CONFIG_CHROMEOS
	/* ChromeOS-specific (0x100 - 0xfff) */
	struct chromeos_acpi chromeos;
#else
	u8	unused2[0x1000 - 0x100];	/* Pad out to 4096 bytes */
#endif
};

#ifdef CONFIG_CHROMEOS
check_member(acpi_global_nvs, chromeos, GNVS_CHROMEOS_ACPI_OFFSET);
#endif

#endif /* _INTEL_GNVS_H_ */
