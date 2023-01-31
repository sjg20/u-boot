# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

#ccflags-y += -DET_DEBUG -DDEBUG

obj-$(CONFIG_PARTITIONS)  += part.o
ifdef CONFIG_BLK
obj-$(CONFIG_PARTITIONS)  += disk-uclass.o
endif

# Must have BLK or SPL_LEGACY_BLOCK to support partitions
ifneq ($(CONFIG_BLK),$(CONFIG_SPL_LEGACY_BLOCK),)
obj-$(CONFIG_MAC_PARTITION)   += part_mac.o
obj-$(CONFIG_DOS_PARTITION)   += part_dos.o
obj-$(CONFIG_ISO_PARTITION)   += part_iso.o
obj-$(CONFIG_AMIGA_PARTITION) += part_amiga.o
obj-$(CONFIG_EFI_PARTITION)   += part_efi.o
endif
