/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2018 Google LLC
 */

#ifndef _CHROMEOS_EFI_X86_PAYLOAD_CONFIG_H
#define _CHROMEOS_EFI_X86_PAYLOAD_CONFIG_H

#include <configs/efi-x86_payload.h>
#include <configs/chromeos.h>

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND	"vboot go auto"

#define xCONFIG_BOOTCOMMAND	\
	"scsi scan; fatload scsi 0:0 01000000 vmlinuz; zboot 01000000"

#endif	/* _CHROMEOS_EFI_X86_PAYLOAD_CONFIG_H */
