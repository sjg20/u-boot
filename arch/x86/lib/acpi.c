// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

#include <common.h>
#include <log.h>
#include <acpi/acpi_table.h>
#include <asm/io.h>
#include <asm/tables.h>

void *acpi_find_wakeup_vector(struct acpi_fadt *fadt)
{
	struct acpi_facs *facs;
	void *wake_vec;

	debug("Trying to find the wakeup vector...\n");

	facs = (struct acpi_facs *)(uintptr_t)fadt->firmware_ctrl;

	if (!facs) {
		debug("No FACS found, wake up from S3 not possible.\n");
		return NULL;
	}

	debug("FACS found at %p\n", facs);
	wake_vec = (void *)(uintptr_t)facs->firmware_waking_vector;
	debug("OS waking vector is %p\n", wake_vec);

	return wake_vec;
}

void enter_acpi_mode(int pm1_cnt)
{
	u16 val = inw(pm1_cnt);

	/*
	 * PM1_CNT register bit0 selects the power management event to be
	 * either an SCI or SMI interrupt. When this bit is set, then power
	 * management events will generate an SCI interrupt. When this bit
	 * is reset power management events will generate an SMI interrupt.
	 *
	 * Per ACPI spec, it is the responsibility of the hardware to set
	 * or reset this bit. OSPM always preserves this bit position.
	 *
	 * U-Boot does not support SMI. And we don't have plan to support
	 * anything running in SMM within U-Boot. To create a legacy-free
	 * system, and expose ourselves to OSPM as working under ACPI mode
	 * already, turn this bit on.
	 */
	outw(val | PM1_CNT_SCI_EN, pm1_cnt);
}
