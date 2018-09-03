# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2018 Google LLC
#

obj-$(CONFIG_$(SPL_)X86) += acpi.o
obj-y += aux-fw-uclass.o
ifndef CONFIG_SPL_BUILD
obj-y += cbmem.o
obj-y += crossystem.o
endif
obj-y += cros_ofnode.o
obj-y += fpmath.o
ifndef CONFIG_SANDBOX
obj-y += jump.o
endif
obj-y += memory.o
obj-y += memwipe.o
obj-y += ranges.o
obj-y += secdata_tpm.o
obj-y += tpm_common.o
obj-y += vboot_internal.o
obj-y += vboot-ec-uclass.o
