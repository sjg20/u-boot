# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2012 The Chromium Authors

obj-y += test-main.o
ifdef CONFIG_SPL_LOAD_FIT
obj-$(CONFIG_SANDBOX) += image/
endif

ifneq ($(CONFIG_BLOBLIST),)
obj-$(CONFIG_CMDLINE) += bloblist.o
obj-$(CONFIG_CMDLINE) += bootm.o
endif
obj-$(CONFIG_CMDLINE) += cmd/
obj-$(CONFIG_CMDLINE) += cmd_ut.o
obj-$(CONFIG_CMDLINE) += command_ut.o
obj-$(CONFIG_UT_COMPRESSION) += compression.o
obj-y += dm/
obj-$(CONFIG_FUZZ) += fuzz/
ifndef CONFIG_SANDBOX_VPL
obj-$(CONFIG_UNIT_TEST) += lib/
endif
obj-$(CONFIG_CMDLINE) += print_ut.o
obj-$(CONFIG_CMDLINE) += str_ut.o
obj-$(CONFIG_UT_TIME) += time_ut.o
obj-y += ut.o

ifeq ($(CONFIG_SPL_BUILD),)
obj-$(CONFIG_UNIT_TEST) += boot/
obj-$(CONFIG_UNIT_TEST) += common/
obj-y += log/
obj-$(CONFIG_UT_UNICODE) += unicode_ut.o
endif
