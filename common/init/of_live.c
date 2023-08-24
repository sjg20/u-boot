// SPDX-License-Identifier: GPL-2.0+
/*
 * Code shared between SPL and U-Boot proper
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootstage.h>
#include <of_live.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

int initr_of_live(void)
{
	if (CONFIG_IS_ENABLED(OF_LIVE)) {
		int ret;

		bootstage_start(BOOTSTAGE_ID_ACCUM_OF_LIVE, "of_live");
		ret = of_live_build(gd->fdt_blob,
				    (struct device_node **)gd_of_root_ptr());
		bootstage_accum(BOOTSTAGE_ID_ACCUM_OF_LIVE);
		if (ret)
			return ret;
	}

	return 0;
}
