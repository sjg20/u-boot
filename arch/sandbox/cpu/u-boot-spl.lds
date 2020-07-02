/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2011-2012 The Chromium OS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Put the tiny devices in the data section since they are changed at runtime */
SECTIONS
{
	. = ALIGN(4);
	.tiny_dev : {
		__tiny_dev_start = .;
		KEEP(*(SORT(.u_boot_list*tiny_dev*)));
		__tiny_dev_end = .;
	}
}

INSERT AFTER .data;

SECTIONS
{
	. = ALIGN(4);
	.u_boot_list : {
		KEEP(*(SORT(.u_boot_list*)));
	}

	__u_boot_sandbox_option_start = .;
	_u_boot_sandbox_getopt : { KEEP(*(.u_boot_sandbox_getopt)) }
	__u_boot_sandbox_option_end = .;
}

INSERT AFTER .data;
