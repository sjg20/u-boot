/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

#ifndef __ASM_INTEL_ACPI_H__
#define __ASM_INTEL_ACPI_H__

struct acpi_ctx;
struct udevice;

int generate_cpu_entries(struct udevice *device, struct acpi_ctx *ctx);

#endif /* __ASM_INTEL_ACPI_H__ */
