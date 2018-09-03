# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2018 Google LLC
#

obj-y += cros_ec_vboot.o
obj-y += ec.o
obj-y += handoff.o
obj-y += legacy.o
obj-y += misc.o
obj-y += tlcl_stub.o
obj-y += utility.o
obj-y += vboot.o
obj-y += vb_data.o

ifndef CONFIG_SPL_BUILD
obj-y += auxfw.o
obj-y += boot_device.o
obj-y += display.o
obj-y += graphics.o
obj-y += keyboard.o
obj-y += screens.o
endif
