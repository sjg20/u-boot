// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <cpu.h>
#include <dm.h>
#include <asm/cpu_common.h>
#include <asm/cpu_x86.h>

struct cpu_apollolake_priv {
};

static int apollolake_get_info(struct udevice *dev, struct cpu_info *info)
{
	return cpu_intel_get_info(info, INTEL_BCLK_MHZ);
}

static int apollolake_get_count(struct udevice *dev)
{
	return 4;
}

static int cpu_x86_apollolake_probe(struct udevice *dev)
{
	return 0;
}

static const struct cpu_ops cpu_x86_apollolake_ops = {
	.get_desc	= cpu_x86_get_desc,
	.get_info	= apollolake_get_info,
	.get_count	= apollolake_get_count,
	.get_vendor	= cpu_x86_get_vendor,
};

static const struct udevice_id cpu_x86_apollolake_ids[] = {
	{ .compatible = "intel,apl-cpu" },
	{ }
};

U_BOOT_DRIVER(cpu_x86_apollolake_drv) = {
	.name		= "cpu_x86_apollolake",
	.id		= UCLASS_CPU,
	.of_match	= cpu_x86_apollolake_ids,
	.bind		= cpu_x86_bind,
	.probe		= cpu_x86_apollolake_probe,
	.ops		= &cpu_x86_apollolake_ops,
	.priv_auto_alloc_size	= sizeof(struct cpu_apollolake_priv),
	.flags		= DM_FLAG_PRE_RELOC,
};
