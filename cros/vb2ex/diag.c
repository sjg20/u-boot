/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Google Inc.
 */

#include <common.h>
#include <cros/storage_info.h>
#include <cros/storage_test.h>
#include <cros/vboot.h>

#define DEFAULT_DIAGNOSTIC_OUTPUT_SIZE (64 * KiB)

vb2_error_t vb2ex_diag_storage_test_control(enum vb2_diag_storage_test ops)
{
	enum BlockDevTestOpsType blockdev_ops;
	switch (ops) {
	case VB2_DIAG_STORAGE_TEST_STOP:
		blockdev_ops = BLOCKDEV_TEST_OPS_TYPE_STOP;
		break;
	case VB2_DIAG_STORAGE_TEST_SHORT:
		blockdev_ops = BLOCKDEV_TEST_OPS_TYPE_SHORT;
		break;
	case VB2_DIAG_STORAGE_TEST_EXTENDED:
		blockdev_ops = BLOCKDEV_TEST_OPS_TYPE_EXTENDED;
		break;
	default:
		return VB2_ERROR_EX_UNIMPLEMENTED;
	}
	return diag_storage_test_control(blockdev_ops);
};
