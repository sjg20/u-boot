/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019 Google LLC
 */

/*
 * board/config.h - configuration options, board-specific
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define xCONFIG_BOOTCOMMAND2	\
	"fatload mmc 1:c 1000000 syslinux/vmlinuz.A; zboot 1000000"

#define yCONFIG_BOOTCOMMAND	\
	"usb start; ext2load usb 0:1 111000 vmlinuz; zboot 111000"

#define CONFIG_BOOTCOMMAND	\
	"read mmc 2:2 100000 0 80; setexpr loader *001004f0; " \
	"setexpr size *00100518; setexpr blocks $size / 200; " \
	"read mmc 2:2 100000 80 $blocks; setexpr setup $loader - 1000; " \
	"setexpr cmdline $loader - 2000; " \
	"zboot start 100000 0 0 0 $setup $cmdline; " \
	"zboot load; zboot setup; zboot dump; zboot go"

#include <configs/x86-common.h>
#include <configs/x86-chromebook.h>

#undef CONFIG_STD_DEVICES_SETTINGS
#define CONFIG_STD_DEVICES_SETTINGS     "stdin=usbkbd,i8042-kbd,serial\0" \
					"stdout=vidconsole,serial\0" \
					"stderr=vidconsole,serial\0"

#define CONFIG_ENV_SECT_SIZE		0x1000
#define CONFIG_ENV_OFFSET		0x003f8000

#define CONFIG_TPL_TEXT_BASE		0xffff8000

#define CONFIG_SYS_NS16550_MEM32
#undef CONFIG_SYS_NS16550_PORT_MAPPED

#endif	/* __CONFIG_H */
