// SPDX-License-Identifier: GPL-2.0+
/*
 * Jumping from SPL to U-Boot proper
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <log.h>
#include <os.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>

int vboot_jump(struct vboot_info *vboot, struct fmap_entry *entry)
{
	struct abuf buf;
	u8 *data;
	int size;
	int ret;

	/*
	 * Allocate a buffer - if using compression, add a margin so that
	 * decompression does not overwrite the compressed data.
	 */
	size = entry->unc_length ? entry->unc_length * 2 : entry->length;
	data = os_malloc(size);
	if (!data)
		return log_msg_ret("alloc", -ENOMEM);
	log_info("Reading firmware offset %x, length %x\n", entry->offset,
		 entry->length);
	abuf_init_set(&buf, data, size);
	ret = fwstore_read_decomp(vboot->fwstore, entry, &buf);
	if (ret)
		return log_msg_ret("read", ret);
	ret = os_jump_to_image(data, size);
	if (ret)
		return log_msg_ret("jump", ret);

	return 0;
}
