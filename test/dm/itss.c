// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for ITSS uclass
 *
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <itss.h>
#include <dm/test.h>
#include <test/ut.h>

/* Base test of the ITSS uclass */
static int dm_test_itss_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_ITSS, &dev));

	ut_asserteq(5, itss_route_pmc_gpio_gpe(dev, 4));
	ut_asserteq(-ENOENT, itss_route_pmc_gpio_gpe(dev, 14));

	ut_assertok(itss_set_irq_polarity(dev, 4, true));
	ut_asserteq(-EINVAL, itss_set_irq_polarity(dev, 14, true));

	return 0;
}
DM_TEST(dm_test_itss_base, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);
