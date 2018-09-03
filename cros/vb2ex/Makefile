# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2021 Google LLC
#

obj-y += crypto.o
obj-y += ec.o
obj-y += misc.o
obj-y += resource.o
obj-y += tpm.o
obj-y += utility.o

ifndef CONFIG_SPL_BUILD
obj-y += altfw.o
obj-y += auxfw.o
obj-y += boot_device.o
obj-y += diag.o
obj-y += display.o
obj-y += keyboard.o
endif
