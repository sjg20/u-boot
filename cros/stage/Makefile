# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2018 Google LLC
#

obj-y += stages.o

# Verification stages (TPL)
ifneq ($(CONFIG_$(SPL_TPL_)CHROMEOS_VBOOT_A),)
obj-y += ver_init.o
obj-y += ver1_vbinit.o
obj-y += ver2_select_fw.o
obj-y += ver3_try_fw.o
obj-y += ver4_locate_fw.o
obj-y += ver5_finish_fw.o
obj-y += ver6_jump_fw.o
endif

ifneq ($(CONFIG_$(SPL_TPL_)CHROMEOS_VBOOT_B),)
obj-y += spl_init.o
obj-y += spl_jump_u_boot.o
endif

ifneq ($(CONFIG_$(SPL_TPL_)CHROMEOS_VBOOT_C),)
obj-y += rw_init.o
obj-y += rw_select_kernel.o
obj-y += rw_lock.o
obj-y += rw_boot_kernel.o
endif
