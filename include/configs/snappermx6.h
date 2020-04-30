/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) Stefano Babic <sbabic@denx.de>
 * Copright 2020 Designa Electronics Ltd
 */


#ifndef __SNAPPERMX6_CONFIG_H
#define __SNAPPERMX6_CONFIG_H

#include "mx6_common.h"
#include "imx6_spl.h"

/* Serial */
#define CONFIG_MXC_UART_BASE	       UART5_BASE

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(8 * SZ_1M)

/* Ethernet */
#define IMX_FEC_BASE			ENET_BASE_ADDR
#define CONFIG_FEC_XCV_TYPE		MII100
#define CONFIG_FEC_FIXED_SPEED			100 /* No autoneg, fix Gb */
#define CONFIG_ETHPRIME			"FEC"
#define CONFIG_FEC_MXC_PHYADDR		0x00

/* Physical Memory Map */
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR
#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM

#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Default environment */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"script=boot.scr\0" \
	"image=zImage\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_addr=0x18000000\0" \
	"initrd_addr=0x13000000\0" \
	"boot_fdt=try\0" \
	"ip_dyn=yes\0" \
	"console=ttymx4\0" \
	"fdt_high=0xffffffff\0"	  \
	"initrd_high=0xffffffff\0" \
	"nfsroot=/export/root\0" \
	"netargs=setenv bootargs console=${console},${baudrate} " \
		"root=/dev/nfs " \
		"ip=dhcp nfsroot=${nfsroot},v3,tcp\0" \
	"netboot=echo Booting from net ...; " \
		"run netargs; " \
		"if test ${ip_dyn} = yes; then " \
			"setenv get_cmd dhcp; " \
		"else " \
			"setenv get_cmd tftp; " \
		"fi; " \
		"${get_cmd} ${image}; " \
		"if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"if ${get_cmd} ${fdt_addr} ${fdt_file}; then " \
				"bootz ${loadaddr} - ${fdt_addr}; " \
			"else " \
				"if test ${boot_fdt} = try; then " \
					"bootz; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"fi; " \
		"else " \
			"bootz; " \
		"fi;\0" \
	"spiargs=setenv bootargs console=${console},${baudrate} " \
		"root=/dev/ram imgset_idx=${imgset_idx}\0" \
	"spiboot=echo Booting from spi ...; " \
		"run spiargs; " \
		"sf probe && " \
		"sf read $loadaddr $kernel_sf_addr 0x780000 && " \
		"bootm $loadaddr\0" \
	"imgset_params_update=echo imgset_idx: ${imgset_idx}; " \
		"if test ${imgset_idx} = 0; then " \
			"setenv sf_env_addr 0xD0000; " \
			"setenv kernel_sf_addr 0x100000; " \
		"elif test ${imgset_idx} = 1; then " \
			"setenv sf_env_addr 0xE0000; " \
			"setenv kernel_sf_addr 0x880000; " \
		"fi;\0" \
	"sf_env_len=0x10000\0" \
	"sf_env_import=sf read ${loadaddr} ${sf_env_addr} ${sf_env_len} && env import -c ${loadaddr} ${sf_env_len}\0" \
	"imgset_bootcmd=run spiboot\0" \
	"boot_active_imgset=echo attempting to load active image set...; " \
		"sf probe; " \
		"for idx in '0 1'; do " \
		  "env default -f -a; " \
		  "setenv imgset_idx ${idx} && run imgset_params_update; " \
		  "run sf_env_import; " \
		  "if test $? = 0 && test ${activeset} = 1; then " \
		    "echo using active env ${imgset_idx} (${sf_env_addr}); " \
		    "run imgset_bootcmd; " \
		  "fi; " \
		"done\0" \
	"boot_inactive_imgset=echo attempting to load inactive image set...; " \
		"sf probe; " \
		"for idx in '0 1'; do " \
		  "env default -f -a; " \
		  "setenv imgset_idx ${idx} && run imgset_params_update; " \
		  "run sf_env_import; " \
		  "if test $? = 0; then " \
		    "echo using inactive env ${imgset_idx} (${sf_env_addr}); " \
		    "run imgset_bootcmd; " \
		  "fi; " \
		"done\0" \
	"boot_default_imgset=echo attempting to load default image set...; " \
		"sf probe; " \
		"env default -f -a; " \
		"setenv imgset_idx 0 && run imgset_params_update; " \
		"run spiboot\0" \
	"boot_imgset=run boot_active_imgset; run boot_inactive_imgset; run boot_default_imgset\0"

#define CONFIG_BOOTCOMMAND  "run boot_imgset"

#define CONFIG_MXC_USB_PORTSC           (PORT_PTS_UTMI | PORT_PTS_PTW)

#endif
