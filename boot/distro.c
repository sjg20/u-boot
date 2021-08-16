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
#include <net.h>
#include <pxe_utils.h>
#include <vsprintf.h>

#define DISTRO_FNAME	"extlinux/extlinux.conf"

/**
 * struct distro_info - useful information for distro_getfile()
 *
 * @bflow: bootflow being booted
 */
struct distro_info {
	struct bootflow *bflow;
};

static int distro_net_getfile(struct pxe_context *ctx, const char *file_path,
			      char *file_addr, ulong *sizep)
{
	char *tftp_argv[] = {"tftp", NULL, NULL, NULL};
	int ret;

	printf("get %s %s\n", file_addr, file_path);
	tftp_argv[1] = file_addr;
	tftp_argv[2] = (void *)file_path;

	if (do_tftpb(ctx->cmdtp, 0, 3, tftp_argv))
		return -ENOENT;
	ret = pxe_get_file_size(sizep);
	if (ret)
		return log_msg_ret("tftp", ret);

	return 0;
}

int distro_net_setup(struct bootflow *bflow)
{
	const char *addr_str;
	char fname[200];
	char *bootdir;
	ulong addr;
	ulong size;
	char *buf;
	int ret;

	addr_str = env_get("pxefile_addr_r");
	if (!addr_str)
		return log_msg_ret("pxeb", -EPERM);
	addr = simple_strtoul(addr_str, NULL, 16);

	bflow->type = BOOTFLOWT_DISTRO;
	ret = pxe_get(addr, &bootdir, &size);
	if (ret)
		return log_msg_ret("pxeb", ret);
	bflow->size = size;

	/* Use the directory of the dhcp bootdir as our subdir, if provided */
	if (bootdir) {
		const char *last_slash;
		int path_len;

		last_slash = strrchr(bootdir, '/');
		if (last_slash) {
			path_len = (last_slash - bootdir) + 1;
			bflow->subdir = malloc(path_len + 1);
			memcpy(bflow->subdir, bootdir, path_len);
			bflow->subdir[path_len] = '\0';
		}
	}
	snprintf(fname, sizeof(fname), "%s%s",
		 bflow->subdir ? bflow->subdir: "", DISTRO_FNAME);

	bflow->fname = strdup(fname);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_LOADED;
	buf = map_sysmem(addr, 0);
	bflow->buf = buf;

	return 0;
}

int distro_boot_setup(struct blk_desc *desc, int partnum,
		      struct bootflow *bflow)
{
	loff_t size, bytes_read;
	ulong addr;
	char *buf;
	int ret;

	bflow->type = BOOTFLOWT_DISTRO;
	bflow->fname = strdup(DISTRO_FNAME);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);
	ret = fs_size(bflow->fname, &size);
	if (ret)
		return log_msg_ret("size", ret);
	bflow->state = BOOTFLOWST_FILE;
	bflow->size = size;
	log_debug("   - distro file size %x\n", (uint)size);
	if (size > 0x10000)
		return log_msg_ret("chk", -E2BIG);

	/* Sadly FS closes the file after fs_size() so we must redo this */
	ret = fs_set_blk_dev_with_part(desc, partnum);
	if (ret)
		return log_msg_ret("set", ret);

	buf = malloc(size + 1);
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
	buf[size] = '\0';
	bflow->state = BOOTFLOWST_LOADED;
	bflow->buf = buf;

	return 0;
}

static int disto_getfile(struct pxe_context *ctx, const char *file_path,
			 char *file_addr, ulong *sizep)
{
	struct distro_info *info = ctx->userdata;
	struct bootflow *bflow = info->bflow;
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	loff_t len_read;
	ulong addr;
	int ret;

	addr = simple_strtoul(file_addr, NULL, 16);
	printf("getfile %lx %s\n", addr, file_path);
	ret = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret)
		return ret;
	ret = fs_read(file_path, addr, 0, 0, &len_read);
	if (ret)
		return ret;
	*sizep = len_read;

	return 0;
}

int distro_boot(struct bootflow *bflow)
{
	struct cmd_tbl cmdtp = {};	/* dummy */
	struct pxe_context ctx;
	struct distro_info info;
	bool is_net = !bflow->blk;
	ulong addr;
	int ret;

	addr = map_to_sysmem(bflow->buf);
	info.bflow = bflow;
	ret = pxe_setup_ctx(&ctx, &cmdtp,
			    is_net ? distro_net_getfile : disto_getfile,
			    &info, !is_net, bflow->subdir);
	if (ret)
		return log_msg_ret("ctx", -EINVAL);

	ret = pxe_process(&ctx, addr, false);
	if (ret)
		return log_msg_ret("bread", -EINVAL);

	return 0;
}
