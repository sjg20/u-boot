# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2016 Google, Inc

obj-$(CONFIG_SPL_BUILD) += spl.o
obj-$(CONFIG_SPL_BUILD) += systemagent.o

ifndef CONFIG_TPL_BUILD
obj-y += cpu.o
obj-y += punit.o
endif

obj-y += gpio.o
obj-y += hostbridge.o
obj-y += itss.o
obj-y += lpc.o
obj-y += pch.o
obj-y += pmc.o
obj-y += uart.o
