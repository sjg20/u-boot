// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <log.h>
#include <cros/vboot.h>

#include <tpm-common.h>

int vboot_ver6_jump_fw(struct vboot_info *vboot)
{
	struct fmap_entry *entry;
	int ret;

	{
		u8 sendbuf[] = {
			0x80, 0x02, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00,
			0x01, 0x4e, 0x40, 0x00, 0x00, 0x0c, 0x01, 0x00,
			0x10, 0x08, 0x00, 0x00, 0x00, 0x09, 0x40, 0x00,
			0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x0d, 0x00, 0x00
		};
		u8 recvbuf[100];
		size_t recv_size;
		int ret;

		recv_size = 100;
		ret = tpm_xfer(vboot->tpm, sendbuf, 0x23, recvbuf, &recv_size);
		printf("\ntry ret=%d\n\n", ret);
		while (ret);
	}


	entry = &vboot->blob->spl_entry;
	ret = vboot_jump(vboot, entry);
	if (ret)
		return log_msg_ret("Jump via fwstore", ret);
	log_info("Ready to jump to firmware\n");

	return 0;
}
