/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

/*
 * board/config.h - configuration options, board specific
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define xCONFIG_BOOTCOMMAND2	\
	"fatload mmc 1:c 1000000 syslinux/vmlinuz.A; zboot 1000000"

#define xCONFIG_BOOTCOMMAND3	\
	"usb start; ext2load usb 0:1 2000000 vmlinuz; zboot 2000000"

#define CONFIG_BOOTCOMMAND	\
	"read mmc 1:2 100000 0 80; setexpr loader *001004f0; " \
	"setexpr size *00100518; setexpr blocks $size / 200; " \
	"read mmc 1:2 100000 80 $blocks; setexpr setup $loader - 1000; " \
	"setexpr cmdline $loader - 2000; " \
	"zboot 100000 0 0 0 $setup $cmdline"

#include <configs/x86-common.h>

#define CONFIG_SYS_MONITOR_LEN		(1 << 20)

#define CONFIG_STD_DEVICES_SETTINGS	"stdin=serial,i8042-kbd,usbkbd\0" \
					"stdout=serial\0" \
					"stderr=serial,vidconsole\0"

/* ATA/IDE support */
#define CONFIG_SYS_IDE_MAXBUS		2
#define CONFIG_SYS_IDE_MAXDEVICE	4
#define CONFIG_SYS_ATA_BASE_ADDR	0
#define CONFIG_SYS_ATA_DATA_OFFSET	0
#define CONFIG_SYS_ATA_REG_OFFSET	0
#define CONFIG_SYS_ATA_ALT_OFFSET	0
#define CONFIG_SYS_ATA_IDE0_OFFSET	0x1f0
#define CONFIG_SYS_ATA_IDE1_OFFSET	0x170
#define CONFIG_ATAPI

#endif	/* __CONFIG_H */
