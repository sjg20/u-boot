// SPDX-License-Identifier: GPL-2.0
/*
 * Writing of vboot state into tables for access by user-space crossystem tool
 *
 * Copyright 2021 Google LLC
 */

#include <common.h>
#include <cros/crossystem.h>
#include <cros/vboot.h>

int crossystem_setup(struct vboot_info *vboot, enum cros_fw_type_t fw_type)
{
	int ret;

	if (IS_ENABLED(CONFIG_X86))
		ret = vboot_update_acpi(vboot, fw_type);
	else
		ret = -ENOSYS;
	if (ret) {
		log_err("Failed to write crossystem tables (err=%d)\n", ret);
		return ret;
	}

	return 0;
}
