// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <acpi.h>

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
