/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015 Google, Inc
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/x86-common.h>

#undef CONFIG_TPM_TIS_BASE_ADDRESS

#define xCONFIG_STD_DEVICES_SETTINGS	"stdin=serial\0" \
					"stdout=vidconsole\0" \
					"stderr=vidconsole\0"

#define CONFIG_STD_DEVICES_SETTINGS	"stdin=serial\0" \
					"stdout=serial\0" \
					"stderr=serial\0"

#undef CONFIG_BOOTCOMMAND

#define CONFIG_BOOTCOMMAND "part list efi 0"

#endif
