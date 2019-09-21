// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <pch.h>

static int apl_pch_probe(struct udevice *dev)
{
	return 0;
}

static const struct pch_ops apl_pch_ops = {
};

static const struct udevice_id apl_pch_ids[] = {
	{ .compatible = "intel,apl-pch" },
	{ }
};

U_BOOT_DRIVER(apl_pch) = {
	.name		= "apl_pch",
	.id		= UCLASS_PCH,
	.of_match	= apl_pch_ids,
	.probe		= apl_pch_probe,
	.ops		= &apl_pch_ops,
};
