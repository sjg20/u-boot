// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for VBE device tree fix-ups
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm/ofnode.h>
#include <linux/libfdt.h>
#include <test/test.h>
#include <test/ut.h>
#include "bootstd_common.h"

/* Basic test of reading nvdata and updating a fwupd node in the device tree */
static int vbe_test_fixup(struct unit_test_state *uts)
{
	ofnode chosen, node;
	oftree tree;

	/*
	 * This test works when called from test_vbe.py and it must use the
	 * flat tree, since device tree fix-ups do not yet support live tree.
	 */
	if (!working_fdt)
		return 0;

	tree = oftree_from_fdt(working_fdt);
	ut_assert(oftree_valid(tree));

	chosen = ofnode_path_root(tree, "/chosen");
	ut_assert(ofnode_valid(chosen));

	node = ofnode_find_subnode(chosen, "random");
	ut_assert(ofnode_valid(node));

	return 0;
}
BOOTSTD_TEST(vbe_test_fixup,
	     UT_TESTF_DM | UT_TESTF_SCAN_FDT | UT_TESTF_FLAT_TREE);
