# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2018 Google LLC
#

obj-y += cros_ec_vboot.o
obj-y += vboot.o
obj-y += vb_data.o

ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_X86) += cb_helper.o
obj-y += graphics.o
obj-y += payload.o
obj-y += vbfile.o
endif
