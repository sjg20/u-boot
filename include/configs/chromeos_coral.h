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

#define xCONFIG_BOOTCOMMAND	"vboot go auto"

#define CONFIG_BOOTCOMMAND	\
	"read mmc 2:2 100000 0 80; setexpr loader *001004f0; " \
	"setexpr size *00100518; setexpr blocks $size / 200; " \
	"read mmc 2:2 100000 80 $blocks; setexpr setup $loader - 1000; " \
	"setexpr cmdline_ptr $loader - 2000; " \
	"setexpr.s cmdline *$cmdline_ptr; " \
	"setexpr cmdline gsub %U \\\\${uuid}; " \
	"echo fred; " \
	"if part uuid mmc 2:2 uuid; then " \
	"zboot start 100000 0 0 0 $setup cmdline; " \
	"zboot load; zboot setup; zboot dump; zboot go;" \
	"fi"

#endif	/* _OS_CONFIG_H */
