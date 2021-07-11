// SPDX-License-Identifier: GPL-2.0+
/*
 * distro boot implementation for bootflow
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootmethod.h>
#include <command.h>
#include <distro.h>
#include <dm.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>
#include <pxe_utils.h>

#define DISTRO_FNAME	"/boot/extlinux/extlinux.conf"

int distro_boot_setup(struct blk_desc *desc, int partnum,
		      struct bootflow *bflow)
{
	loff_t size, bytes_read;
	ulong addr;
	void *buf;
	int ret;

	bflow->type = BOOTFLOWT_DISTRO;
	bflow->fname = strdup(DISTRO_FNAME);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);
	ret = fs_size(bflow->fname, &size);
	if (ret)
		return log_msg_ret("size", ret);
	bflow->state = BOOTFLOWST_FILE;
	log_debug("   - distro file size %x\n", (uint)size);
	if (size > 0x10000)
		return log_msg_ret("chk", -E2BIG);

	// FIXME: FS closes the file after fs_size()
	ret = fs_set_blk_dev_with_part(desc, partnum);
	if (ret)
		return log_msg_ret("set", ret);

	buf = malloc(size);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	addr = map_to_sysmem(buf);

	ret = fs_read(bflow->fname, addr, 0, 0, &bytes_read);
	if (ret) {
		free(buf);
		return log_msg_ret("read", ret);
	}
	if (size != bytes_read)
		return log_msg_ret("bread", -EINVAL);
	bflow->state = BOOTFLOWST_LOADED;
	bflow->buf = buf;

	return 0;
}

static int disto_getfile(struct pxe_context *ctx, const char *file_path,
			 char *file_addr, ulong *sizep)
{
	printf("get file %s %s\n", file_path, file_addr);
	//TODO

	return 0;
}

int distro_boot(struct bootflow *bflow)
{
	struct cmd_tbl cmdtp = {};	/* dummy */
	struct pxe_context ctx;
	ulong addr;
	int ret;

	addr = map_to_sysmem(bflow->buf);
	ret = pxe_setup_ctx(&ctx, &cmdtp, disto_getfile, NULL, true,
			    bflow->fname);
	if (ret)
		return log_msg_ret("ctx", -EINVAL);

	ret = pxe_process(&ctx, addr, false);
	if (ret)
		return log_msg_ret("bread", -EINVAL);

	return 0;
}
