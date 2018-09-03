# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2018 Google LLC
#

obj-$(CONFIG_$(SPL_)X86) += acpi.o
obj-y += aux-fw-uclass.o
obj-y += cros_ofnode.o
obj-y += cros_tpm.o
ifndef CONFIG_SANDBOX
obj-y += jump.o
endif
obj-y += memwipe.o
obj-y += resource.o
obj-y += vboot_internal.o
obj-y += vboot-ec-uclass.o
