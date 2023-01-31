/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 */
#ifndef _X86_ASM_SYSRESET_H_
#define _X86_ASM_SYSRESET_H_

#include <dt-structs.h>

struct x86_sysreset_plat {
#if IS_ENABLED(CONFIG_OF_PLATDATA)
	struct dtd_x86_reset dtplat;
#endif

	struct udevice *pch;
};

#endif	/* _X86_ASM_SYSRESET_H_ */
