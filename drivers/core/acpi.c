// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define DEBUG

#include <common.h>
#include <acpi.h>
#include <dm.h>
#include <dm/root.h>

int acpi_return_name(char *out_name, const char *name)
{
	strcpy(out_name, name);

	return 0;
}

int ctx_align(struct acpi_ctx *ctx)
{
	ctx->current = ALIGN(ctx->current, 16);

	return 0;
}

int _acpi_dev_write_tables(struct udevice *parent, struct acpi_ctx *ctx)
{
	struct acpi_ops *aops;
	struct udevice *dev;
	int ret;

	aops = device_get_acpi_ops(parent);
	if (aops) {
		debug("- %s\n", parent->name);
		ret = aops->write_tables(parent, ctx);
		if (ret)
			return ret;
	}
	device_foreach_child(dev, parent) {
		ret = _acpi_dev_write_tables(dev, ctx);
		if (ret)
			return ret;
	}

	return 0;
}

int acpi_dev_write_tables(struct acpi_ctx *ctx)
{
	int ret;

	debug("Writing device tables\n");
	ret = _acpi_dev_write_tables(dm_root(), ctx);
	debug("Writing finished, err=%d\n", ret);

	return ret;
}
