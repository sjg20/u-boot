/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2018 Google LLC
 */

#ifndef _CHROMEOS_COREBOOT_CONFIG_H
#define _CHROMEOS_COREBOOT_CONFIG_H

#include <configs/coreboot.h>
#include <configs/chromeos.h>

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND	"vboot go auto"

#endif	/* _CHROMEOS_COREBOOT_CONFIG_H */
