# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2004-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

ifndef CONFIG_SPL_BUILD

obj-$(CONFIG_BOOT_RETRY) += bootretry.o
obj-$(CONFIG_CMD_BOOTM) += bootm.o bootm_os.o
obj-$(CONFIG_CMD_BOOTZ) += bootm.o bootm_os.o
obj-$(CONFIG_CMD_BOOTI) += bootm.o bootm_os.o

obj-$(CONFIG_PXE_UTILS) += pxe_utils.o

endif

obj-y += image.o image-board.o
obj-$(CONFIG_ANDROID_AB) += android_ab.o
obj-$(CONFIG_ANDROID_BOOT_IMAGE) += image-android.o image-android-dt.o

obj-$(CONFIG_BOOTSTD) += bootdev-uclass.o
obj-$(CONFIG_BOOTSTD) += bootflow.o
obj-$(CONFIG_BOOTSTD) += bootmeth-uclass.o
obj-$(CONFIG_BOOTSTD) += bootstd-uclass.o

obj-$(CONFIG_BOOTMETH_DISTRO) += bootmeth_distro.o
obj-$(CONFIG_BOOTMETH_DISTRO_PXE) += bootmeth_pxe.o
obj-$(CONFIG_BOOTMETH_EFILOADER) += bootmeth_efi.o
obj-$(CONFIG_BOOTMETH_SANDBOX) += bootmeth_sandbox.o
obj-$(CONFIG_BOOTMETH_SCRIPT) += bootmeth_script.o
ifdef CONFIG_BOOTSTD_FULL
obj-$(CONFIG_CMD_BOOTEFI_BOOTMGR) += bootmeth_efi_mgr.o
obj-$(CONFIG_BOOTSTD) += bootflow_menu.o
endif

obj-$(CONFIG_OF_LIBFDT) += image-fdt.o
obj-$(CONFIG_FIT_SIGNATURE) += fdt_region.o
obj-$(CONFIG_FIT) += image-fit.o
obj-$(CONFIG_MULTI_DTB_FIT) += boot_fit.o common_fit.o
obj-$(CONFIG_IMAGE_PRE_LOAD) += image-pre-load.o
obj-$(CONFIG_IMAGE_SIGN_INFO) += image-sig.o
obj-$(CONFIG_FIT_SIGNATURE) += image-fit-sig.o
obj-$(CONFIG_FIT_CIPHER) += image-cipher.o

obj-$(CONFIG_CMD_ADTIMG) += image-android-dt.o

ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_SPL_LOAD_FIT) += common_fit.o
endif

obj-$(CONFIG_BOOTMETH_VBE) += expo.o scene.o scene_menu.o

obj-$(CONFIG_BOOTMETH_VBE) += vbe.o
obj-$(CONFIG_BOOTMETH_VBE_REQUEST) += vbe_request.o
obj-$(CONFIG_BOOTMETH_VBE_SIMPLE) += vbe_simple.o
obj-$(CONFIG_BOOTMETH_VBE_SIMPLE_FW) += vbe_simple_fw.o
obj-$(CONFIG_BOOTMETH_VBE_SIMPLE_OS) += vbe_simple_os.o
