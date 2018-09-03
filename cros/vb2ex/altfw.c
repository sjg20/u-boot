// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of callbacks for 'legacy' boot
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <os.h>
#include <cros/vboot.h>

vb2_error_t vb2ex_run_altfw(u32 altfw_num)
{
	/* TODO(sjg@chromium.org): Implement this */
	printf("Legacy boot %d\n", altfw_num);

	return 1;
}

u32 vb2ex_get_altfw_count(void)
{
	/* TODO(sjg@chromium.org): Implement this */
	return 2;
}
