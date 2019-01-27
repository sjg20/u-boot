// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Google Inc,
 * Written by Simon Glass <sjg@chromium.org>
 *
 * Reset driver for intel x86 processors with a PCH. Supports powering the
 * device off.
 */

#include <common.h>
#include <dm.h>
#include <sysreset.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/arch/pch.h>
#include <asm/arch/pm.h>

struct x86_reset_platdata {
	struct udevice *pch;
};

/*
 * Power down the machine by using the power management sleep control
 * of the chipset. This will currently only work on Intel chipsets.
 * However, adapting it to new chipsets is fairly simple. You will
 * have to find the IO address of the power management register block
 * in your southbridge, and look up the appropriate SLP_TYP_S5 value
 * from your southbridge's data sheet.
 *
 * This function never returns.
 */
int pch_sysreset_power_off(struct udevice *dev)
{
	struct x86_reset_platdata *plat = dev_get_platdata(dev);
	u16 pmbase;
	u32 reg32;
	int ret;

	if (!plat->pch)
		return -ENOENT;

	/* Find the base address of the powermanagement registers */
	ret = dm_pci_read_config16(plat->pch, 0x40, &pmbase);
	if (ret)
		return ret;

	pmbase &= 0xfffe;

	/* Mask interrupts or system might stay in a coma
	 * (not executing code anymore, but not powered off either)
	 */
	asm("cli");

	/*
	 * Avoid any GPI waking the system from S5* or the system might stay in
	 * a coma
	 */
	outl(0x00000000, pmbase + GPE0_EN(0));

	/* Clear Power Button Status */
	outw(PWRBTN_STS, pmbase + PM1_STS);

	/* PMBASE + 4, Bit 10-12, Sleeping Type, * set to 111 -> S5, soft_off */
	reg32 = inl(pmbase + PM1_CNT);

	/* Set Sleeping Type to S5 (poweroff) */
	reg32 &= ~(SLP_EN | SLP_TYP);
	reg32 |= SLP_TYP_S5;
	outl(reg32, pmbase + PM1_CNT);

	/* Now set the Sleep Enable bit */
	reg32 |= SLP_EN;
	outl(reg32, pmbase + PM1_CNT);

	for (;;)
		asm("hlt");
}

static int pch_sysreset_request(struct udevice *dev, enum sysreset_t type)
{
	int ret;

	switch (type) {
	case SYSRESET_POWER_OFF:
		ret = pch_sysreset_power_off(dev);
		if (ret)
			return ret;
		break;
	default:
		return -ENOSYS;
	}

	return -EINPROGRESS;
}

static int pch_sysreset_ofdata_to_platdata(struct udevice *dev)
{
	struct x86_reset_platdata *plat = dev_get_platdata(dev);
	int ret;

	ret = uclass_get_device_by_phandle(UCLASS_PCH, dev, "intel,pch",
					   &plat->pch);
	if (ret && ret != -ENOENT)
		return log_ret(ret);

	return 0;
}

static const struct udevice_id pch_sysreset_ids[] = {
	{ .compatible = "intel,pch-reset" },
	{ }
};

static struct sysreset_ops pch_sysreset_ops = {
	.request = pch_sysreset_request,
};

U_BOOT_DRIVER(pch_sysreset) = {
	.name = "pch-sysreset",
	.id = UCLASS_SYSRESET,
	.of_match = pch_sysreset_ids,
	.ops = &pch_sysreset_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.ofdata_to_platdata	= pch_sysreset_ofdata_to_platdata,
};
