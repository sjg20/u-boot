// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for bootdev functions. All start with 'bootdev'
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootstd.h>
#include <dm.h>
#include <test/suites.h>
#include <test/ut.h>
#include "bootdev_common.h"

int bootdev_test_drop_boot_order(struct unit_test_state *uts)
{
	struct bootstd_priv *priv;
	struct udevice *bootstd;

	ut_assertok(uclass_first_device_err(UCLASS_BOOTSTD, &bootstd));
	priv = dev_get_priv(bootstd);
	priv->order = NULL;

	return 0;
}
