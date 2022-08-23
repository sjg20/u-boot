// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * This file contains dummy implementations of SCSI functions requried so
 * that CONFIG_SCSI can be enabled for sandbox.
 */

#include <common.h>
#include <dm.h>
#include <os.h>
#include <malloc.h>
#include <scsi.h>
#include <scsi_emul.h>

enum {
	SANDBOX_SCSI_BLOCK_LEN		= 512,
	SANDBOX_SCSI_BUF_SIZE		= 512,
};

/**
 * struct sandbox_scsi_priv
 *
 * @eminfo: emulator state
 * @fd: File descriptor of backing file
 */
struct sandbox_scsi_priv {
	struct scsi_emul_info eminfo;
	int fd;
};

struct sandbox_scsi_plat {
	const char *pathname;
};

static int sandbox_scsi_exec(struct udevice *dev, struct scsi_cmd *req)
{
	struct sandbox_scsi_priv *priv = dev_get_priv(dev);
	struct scsi_emul_info *info = &priv->eminfo;
	int ret;

	ret = sb_scsi_emul_command(info, req, req->cmdlen);
	if (ret) {
		debug("SCSI command 0x%02x ret errno %d\n", req->cmd[0], ret);
		return ret;
	}

	return 0;
}

static int sandbox_scsi_bus_reset(struct udevice *dev)
{
	/* Not implemented */

	return 0;
}

static int sandbox_scsi_probe(struct udevice *dev)
{
	struct scsi_plat *scsi_plat = dev_get_uclass_plat(dev);
	struct sandbox_scsi_plat *plat = dev_get_plat(dev);
	struct sandbox_scsi_priv *priv = dev_get_priv(dev);
	struct scsi_emul_info *info = &priv->eminfo;
	int ret;

	scsi_plat->max_id = 2;
	scsi_plat->max_lun = 3;
	scsi_plat->max_bytes_per_req = 1 << 20;

	info->vendor = "SANDBOX";
	info->product = "FAKE DISK";
	info->block_size = SANDBOX_SCSI_BLOCK_LEN;
	priv->fd = os_open(plat->pathname, OS_O_RDONLY);
	if (priv->fd != -1) {
		ret = os_get_filesize(plat->pathname, &info->file_size);
		if (ret)
			return log_msg_ret("sz", ret);
	}
	info->buff = malloc(SANDBOX_SCSI_BUF_SIZE);
	if (!info->buff)
		return log_ret(-ENOMEM);

	return 0;
}

static int sandbox_scsi_of_to_plat(struct udevice *dev)
{
	struct sandbox_scsi_plat *plat = dev_get_plat(dev);

	plat->pathname = dev_read_string(dev, "sandbox,filepath");

	return 0;
}

struct scsi_ops sandbox_scsi_ops = {
	.exec		= sandbox_scsi_exec,
	.bus_reset	= sandbox_scsi_bus_reset,
};

static const struct udevice_id sanbox_scsi_ids[] = {
	{ .compatible = "sandbox,scsi" },
	{ }
};

U_BOOT_DRIVER(sandbox_scsi) = {
	.name		= "sandbox_scsi",
	.id		= UCLASS_SCSI,
	.ops		= &sandbox_scsi_ops,
	.of_match	= sanbox_scsi_ids,
	.of_to_plat	= sandbox_scsi_of_to_plat,
	.probe		= sandbox_scsi_probe,
	.plat_auto	= sizeof(struct sandbox_scsi_plat),
	.priv_auto	= sizeof(struct sandbox_scsi_priv),
};
