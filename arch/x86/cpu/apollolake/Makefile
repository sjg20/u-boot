# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2016 Google, Inc

obj-$(CONFIG_SPL_BUILD) += cpu_spl.o
obj-$(CONFIG_SPL_BUILD) += spl.o
obj-$(CONFIG_SPL_BUILD) += systemagent.o

ifndef CONFIG_TPL_BUILD
obj-y += cpu.o
obj-y += punit.o
ifdef CONFIG_SPL_BUILD
obj-y += fsp_m.o
endif
endif
ifndef CONFIG_SPL_BUILD
obj-y += fsp_s.o
endif

obj-y += gpio.o
obj-y += hostbridge.o
obj-y += itss.o
obj-y += lpc.o
obj-y += lpss.o
obj-y += p2sb.o
obj-y += pch.o
obj-y += pmc.o
obj-y += uart.o
