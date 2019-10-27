/*
 * This file is part of the coreboot project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2, or (at your option)
 * any later version of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <binman.h>
#include <dm.h>
#include <spi_flash.h>
#include <asm/intel_opregion.h>

static char vbt_data[8 << 10];

static int locate_vbt(char **vbtp, int *sizep)
{
	struct binman_entry vbt;
	struct udevice *dev;
	int size;
	int ret;

	ret = binman_entry_find("intel-vbt", &vbt);
	if (ret)
		return log_msg_ret("find VBT", ret);
	ret = uclass_first_device_err(UCLASS_SPI_FLASH, &dev);
	if (ret)
		return log_msg_ret("find flash", ret);
	printf("pos %x\n", vbt.image_pos);
	size = vbt.size;
	if (size > sizeof(vbt_data))
		return log_msg_ret("vbt", -E2BIG);
	ret = spi_flash_read_dm(dev, vbt.image_pos, size, vbt_data);
	if (ret)
		return log_msg_ret("read", ret);

	uint32_t vbtsig = 0;

	memcpy(&vbtsig, vbt_data, sizeof(vbtsig));
	if (vbtsig != VBT_SIGNATURE) {
		log_err("Missing/invalid signature in VBT data file!\n");
		return -EINVAL;
	}

	log_info("Found a VBT of %u bytes\n", size);
	*sizep = size;
	*vbtp = vbt_data;

	return 0;
}

/* Write ASLS PCI register and prepare SWSCI register. */
static int intel_gma_opregion_register(struct udevice *dev, ulong opregion)
{
	int sci_reg;

	if (!device_active(dev))
		return -ENOENT;

	/*
	 * Intel BIOS Specification
	 * Chapter 5.3.7 "Initialize Hardware State"
	 */
	dm_pci_write_config32(dev, ASLS, opregion);

	/*
	 * Atom-based platforms use a combined SMI/SCI register,
	 * whereas non-Atom platforms use a separate SCI register.
	 */
	if (IS_ENABLED(CONFIG_INTEL_GMA_SWSMISCI))
		sci_reg = SWSMISCI;
	else
		sci_reg = SWSCI;

	/*
	 * Intel's Windows driver relies on this:
	 * Intel BIOS Specification
	 * Chapter 5.4 "ASL Software SCI Handler"
	 */
	dm_pci_clrset_config16(dev, sci_reg, GSSCIE, SMISCISEL);

	return 0;
}

/* Initialise IGD OpRegion, called from ACPI code and OS drivers */
int intel_gma_init_igd_opregion(struct udevice *dev,
				struct igd_opregion *opregion)
{
	struct optionrom_vbt *vbt = NULL;
	char *vbt_buf;
	int vbt_size;
	int ret;

	ret = locate_vbt(&vbt_buf, &vbt_size);
	if (ret) {
		log_err("GMA: VBT couldn't be found\n");
		return log_msg_ret("find vbt", ret);
	}

	memset(opregion, '\0', sizeof(struct igd_opregion));

	memcpy(&opregion->header.signature, IGD_OPREGION_SIGNATURE,
		sizeof(opregion->header.signature));
	memcpy(opregion->header.vbios_version, vbt->coreblock_biosbuild,
					ARRAY_SIZE(vbt->coreblock_biosbuild));
	/* Extended VBT support */
	if (vbt->hdr_vbt_size > sizeof(opregion->vbt.gvd1)) {
		log_err("GMA: Unable to add Ext VBT to cbmem\n");
		return -E2BIG;
	} else {
		/* Raw VBT size which can fit in gvd1 */
		memcpy(opregion->vbt.gvd1, vbt, vbt->hdr_vbt_size);
	}

	/* 8kb */
	opregion->header.size = sizeof(struct igd_opregion) / 1024;

	/*
	 * Left-shift version field to accommodate Intel Windows driver quirk
	 * when not using a VBIOS.
	 * Required for Legacy boot + NGI, UEFI + NGI, and UEFI + GOP driver.
	 *
	 * Tested on: (platform, GPU, windows driver version)
	 * samsung/stumpy (SNB, GT2, 9.17.10.4459)
	 * google/link (IVB, GT2, 15.33.4653)
	 * google/wolf (HSW, GT1, 15.40.36.4703)
	 * google/panther (HSW, GT2, 15.40.36.4703)
	 * google/rikku (BDW, GT1, 15.40.36.4703)
	 * google/lulu (BDW, GT2, 15.40.36.4703)
	 * google/chell (SKL-Y, GT2, 15.45.21.4821)
	 * google/sentry (SKL-U, GT1, 15.45.21.4821)
	 * purism/librem13v2 (SKL-U, GT2, 15.45.21.4821)
	 *
	 * No adverse effects when using VBIOS or booting Linux.
	 */
	opregion->header.version = IGD_OPREGION_VERSION << 24;

	// FIXME We just assume we're mobile for now
	opregion->header.mailboxes = MAILBOXES_MOBILE;

	// TODO Initialize Mailbox 1
	opregion->mailbox1.clid = 1;

	// TODO Initialize Mailbox 3
	opregion->mailbox3.bclp = IGD_BACKLIGHT_BRIGHTNESS;
	opregion->mailbox3.pfit = IGD_FIELD_VALID | IGD_PFIT_STRETCH;
	opregion->mailbox3.pcft = 0; // should be (IMON << 1) & 0x3e
	opregion->mailbox3.cblv = IGD_FIELD_VALID | IGD_INITIAL_BRIGHTNESS;
	opregion->mailbox3.bclm[0] = IGD_WORD_FIELD_VALID + 0x0000;
	opregion->mailbox3.bclm[1] = IGD_WORD_FIELD_VALID + 0x0a19;
	opregion->mailbox3.bclm[2] = IGD_WORD_FIELD_VALID + 0x1433;
	opregion->mailbox3.bclm[3] = IGD_WORD_FIELD_VALID + 0x1e4c;
	opregion->mailbox3.bclm[4] = IGD_WORD_FIELD_VALID + 0x2866;
	opregion->mailbox3.bclm[5] = IGD_WORD_FIELD_VALID + 0x327f;
	opregion->mailbox3.bclm[6] = IGD_WORD_FIELD_VALID + 0x3c99;
	opregion->mailbox3.bclm[7] = IGD_WORD_FIELD_VALID + 0x46b2;
	opregion->mailbox3.bclm[8] = IGD_WORD_FIELD_VALID + 0x50cc;
	opregion->mailbox3.bclm[9] = IGD_WORD_FIELD_VALID + 0x5ae5;
	opregion->mailbox3.bclm[10] = IGD_WORD_FIELD_VALID + 0x64ff;

	/* Write ASLS PCI register and prepare SWSCI register. */
	ret = intel_gma_opregion_register(dev, (ulong)opregion);
	if (ret)
		return log_msg_ret("write asls", ret);

	return 0;
}
