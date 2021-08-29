/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2018 Google LLC
 */

#ifndef _CHROMEOS_SANDBOX_CONFIG_H
#define _CHROMEOS_SANDBOX_CONFIG_H

#define SANDBOX_SERIAL_SETTINGS		"stdin=serial,cros-ec-keyb,usbkbd\0" \
					"stdout=serial\0" \
					"stderr=serial,vidconsole\0"
#include <configs/sandbox.h>
#include <configs/chromeos.h>

#endif	/* _CHROMEOS_SANDBOX_CONFIG_H */
