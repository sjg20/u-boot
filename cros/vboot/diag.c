// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of diagnostics callbacks
 *
 * Copyright 2021 Google LLC
 */

#define LOG_CATEGORY UCLASS_VBOOT

#include <common.h>
#include <cros/vboot.h>

vb2_error_t vb2ex_diag_get_storage_health(const char **out)
{
	return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
}

vb2_error_t vb2ex_diag_get_storage_test_log(const char **out)
{
	return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
}

vb2_error_t vb2ex_diag_memory_quick_test(int reset, const char **out)
{
	return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
}

vb2_error_t vb2ex_diag_memory_full_test(int reset, const char **out)
{
	return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
}

vb2_error_t vb2ex_diag_storage_test_control(enum vb2_diag_storage_test ops)
{
	return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
}

