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
#include <scsi.h>

static int sandbox_scsi_inquiry(struct udevice *dev, struct scsi_cmd *req)
{
	struct scsi_inquiry_resp *resp = (struct scsi_inquiry_resp *)req->cmd;
	u8 *cmd = req->cmd;
	int lun;

	lun = cmd[SCSI_LUN] >> SCSI_LUN_SHIFT;
	printf("target = %d, lun = %d\n", req->target, lun);

	/* target ID 0 has only lun 0; target ID 1 has only lun 2 */
	if (!req->target ? lun != 0 : lun != 2) {
		req->contr_stat = SCSI_SEL_TIME_OUT;
		return -EIO;
	}

	strcpy(resp->vendor, "SANDBOX ");
	strcpy(resp->product, !req->target ? "FAKE DISK       " :
	       "LESS REAL DISK  ");

	return 0;
}

static int sandbox_scsi_exec(struct udevice *dev, struct scsi_cmd *req)
{
	int ret;

	switch (req->cmd[0]) {
/*
	case SCSI_READ16:
	case SCSI_READ10:
		ret = ata_scsiop_read_write(uc_priv, req, 0);
		break;
	case SCSI_WRITE10:
		ret = ata_scsiop_read_write(uc_priv, req, 1);
		break;
	case SCSI_RD_CAPAC10:
		ret = ata_scsiop_read_capacity10(uc_priv, req);
		break;
	case SCSI_RD_CAPAC16:
		ret = ata_scsiop_read_capacity16(uc_priv, req);
		break;
	case SCSI_TST_U_RDY:
		ret = ata_scsiop_test_unit_ready(uc_priv, req);
		break;
*/
	case SCSI_INQUIRY:
		ret = sandbox_scsi_inquiry(dev, req);
		break;
	default:
		printf("Unsupport SCSI command 0x%02x\n", req->cmd[0]);
		return -ENOTSUPP;
	}

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

	scsi_plat->max_id = 2;
	scsi_plat->max_lun = 3;
	scsi_plat->max_bytes_per_req = 1 << 20;

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
	.probe		= sandbox_scsi_probe,
};
