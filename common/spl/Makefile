# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2012
# Texas Instruments Incorporated - http://www.ti.com/
# Aneesh V <aneesh@ti.com>
# Based on common/Makefile.
#

ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_FRAMEWORK) += spl.o
obj-$(CONFIG_BOOTROM_SUPPORT) += spl_bootrom.o
obj-$(CONFIG_LOAD_FIT) += spl_fit.o
obj-$(CONFIG_LEGACY_IMAGE_FORMAT) += spl_legacy.o
obj-$(CONFIG_NOR_SUPPORT) += spl_nor.o
obj-$(CONFIG_XIP_SUPPORT) += spl_xip.o
obj-$(CONFIG_YMODEM_SUPPORT) += spl_ymodem.o
ifndef CONFIG_SPL_UBI
obj-$(CONFIG_NAND_SUPPORT) += spl_nand.o
obj-$(CONFIG_ONENAND_SUPPORT) += spl_onenand.o
endif
obj-$(CONFIG_UBI) += spl_ubi.o
obj-$(CONFIG_NET) += spl_net.o
obj-$(CONFIG_MMC) += spl_mmc.o
obj-$(CONFIG_ATF) += spl_atf.o
obj-$(CONFIG_OPTEE_IMAGE) += spl_optee.o
obj-$(CONFIG_OPENSBI) += spl_opensbi.o
obj-$(CONFIG_USB_STORAGE) += spl_usb.o
obj-$(CONFIG_FS_FAT) += spl_fat.o
obj-$(CONFIG_FS_EXT4) += spl_ext.o
obj-$(CONFIG_SATA) += spl_sata.o
obj-$(CONFIG_SEMIHOSTING) += spl_semihosting.o
obj-$(CONFIG_DFU) += spl_dfu.o
obj-$(CONFIG_SPI_LOAD) += spl_spi.o
obj-$(CONFIG_RAM_SUPPORT) += spl_ram.o
obj-$(CONFIG_USB_SDP_SUPPORT) += spl_sdp.o
endif
