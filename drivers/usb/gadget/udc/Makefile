# SPDX-License-Identifier: GPL-2.0+
#
# USB peripheral controller drivers

ifndef CONFIG_DM_USB_GADGET
obj-$(CONFIG_USB_DWC3_GADGET)	+= udc-core.o
endif

obj-$(CONFIG_DM_USB_GADGET)	+= udc-core.o
obj-$(CONFIG_DM) += udc-uclass.o
