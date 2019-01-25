// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __configs_chromeos_samus_h__
#define __configs_chromeos_samus_h__

#define CONFIG_BOOTCOMMAND	"vboot go auto"

#include <configs/chromebook_samus.h>

#include <configs/chromeos.h>

#undef CONFIG_STD_DEVICES_SETTINGS
#define CONFIG_STD_DEVICES_SETTINGS     "stdin=usbkbd,i8042-kbd,serial\0" \
					"stdout=serial\0" \
					"stderr=serial\0"

#define CONFIG_SPL_TEXT_BASE		0xffe70000
#define CONFIG_TPL_TEXT_BASE		0xfffd8000

#endif /* __configs_chromeos_samus_h__ */
