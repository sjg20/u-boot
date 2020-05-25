// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 Google, Inc
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <handoff.h>
#include <init.h>
#include <asm/cpu_common.h>
#include <asm/intel_regs.h>
#include <asm/lapic.h>
#include <asm/lpc_common.h>
#include <asm/msr.h>
#include <asm/mtrr.h>
#include <asm/post.h>
#include <asm/microcode.h>

DECLARE_GLOBAL_DATA_PTR;

int arch_cpu_init(void)
{
	int ret;

#if CONFIG_IS_ENABLED(HANDOFF) && IS_ENABLED(CONFIG_USE_HOB)
	struct spl_handoff *ho = gd->spl_handoff;

	if (IS_ENABLED(CONFIG_APL_FROM_EARLY_RAMSTAGE)) {
		gd->arch.hob_list = (void *)0x7ac1e000;
		printf("\n\nHacking hob list to %p\n", gd->arch.hob_list);
	} else {
		gd->arch.hob_list = ho->arch.hob_list;
	}
#endif
	ret = x86_cpu_reinit_f();

	return ret;
}
