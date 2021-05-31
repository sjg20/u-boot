# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2018 Google LLC
#

obj-y += altfw.o
obj-y += cros_ec_vboot.o
obj-y += ec.o
obj-y += handoff.o
obj-y += misc.o
obj-y += tlcl_stub.o
obj-y += utility.o
obj-y += vboot.o
obj-y += vb_data.o

ifndef CONFIG_SPL_BUILD
obj-y += auxfw.o
obj-y += boot_device.o
obj-$(CONFIG_X86) += cb_helper.o
obj-y += display.o
obj-y += graphics.o
obj-y += keyboard.o
obj-y += payload.o
#obj-y += screens.o
obj-y += vbfile.o
endif
