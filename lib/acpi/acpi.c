// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <acpi.h>

int acpi_return_name(char *out_name, name)
{
	strcpy(out_name, name);

	return 0;
}
