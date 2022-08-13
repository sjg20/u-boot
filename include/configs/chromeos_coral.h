/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019 Google LLC
 */

/*
 * board/config.h - configuration options, board-specific
 */

#ifndef _OS_CONFIG_H
#define _OS_CONFIG_H

#define TODO

#include <configs/chromeos.h>

#include <configs/chromebook_coral.h>

#undef CONFIG_BOOTCOMMAND

#define CONFIG_BOOTCOMMAND	"vboot go auto"

#endif	/* _OS_CONFIG_H */
