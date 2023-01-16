# SPDX-License-Identifier: GPL-2.0+

ifeq ($(CONFIG_EFI_APP),)
ifdef CONFIG_X86_64
head-y := arch/x86/cpu/start64.o
else
ifeq ($(CONFIG_X86_16BIT_INIT),y)
head-y := arch/x86/cpu/start.o
else
ifndef CONFIG_SPL
head-y := arch/x86/cpu/start.o
else
ifdef CONFIG_SPL_BUILD
head-y	= arch/x86/cpu/start_from_tpl.o
else
head-y	= arch/x86/cpu/start_from_spl.o
endif
endif
endif
endif
endif # EFI

head-$(CONFIG_X86_16BIT_INIT) += arch/x86/cpu/start16.o
head-$(CONFIG_X86_16BIT_INIT) += arch/x86/cpu/resetvec.o

libs-y += arch/x86/cpu/
libs-y += arch/x86/lib/
