// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <log.h>
#include <cros/vboot.h>

int vboot_ver6_jump_fw(struct vboot_info *vboot)
{
	struct fmap_entry *entry;
	int ret;

	entry = &vboot->blob->spl_entry;
	ret = vboot_jump(vboot, entry);
	if (ret)
		return log_msg_ret("Jump via fwstore", ret);
	log_info("Ready to jump to firmware\n");

	return 0;
}
