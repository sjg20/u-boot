# SPDX-License-Identifier: GPL-2.0+
#

obj-$(CONFIG_$(SPL_)ACPIGEN) += acpigen.o
obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi_device.o
obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi_dp.o
obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi_table.o
obj-y += acpi_writer.o

# With QEMU the ACPI tables come from there, not from U-Boot
ifndef CONFIG_QEMU
obj-$(CONFIG_ACPI) += base.o
obj-$(CONFIG_ACPI) += csrt.o
obj-$(CONFIG_ACPI) += mcfg.o

# Sandbox does not build a .asl file
ifndef CONFIG_SANDBOX
obj-$(CONFIG_ACPI) += dsdt.o
endif

obj-$(CONFIG_ACPI) += facs.o
obj-$(CONFIG_ACPI) += ssdt.o
endif
