/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * 32-bit x86 Startup Code when running from SPL
 *
 * Copyright 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <config.h>

.section .text.start
.code32
.globl _start
.type _start, @function
_start:
	/* Set up memory using the existing stack */
	mov	%esp, %eax
	call	board_init_f_alloc_reserve
	mov	%eax, %esp

	call	board_init_f_init_reserve

	call	board_init_f
	call	board_init_f_r

	/* Should not return here */
	jmp	.

.globl board_init_f_r_trampoline
.type board_init_f_r_trampoline, @function
board_init_f_r_trampoline:
	/*
	 * SPL has been executed and SDRAM has been initialised, U-Boot code
	 * has been copied into RAM, BSS has been cleared and relocation
	 * adjustments have been made. It is now time to jump into the in-RAM
	 * copy of U-Boot
	 *
	 * %eax = Address of top of new stack
	 */

	/* Stack grows down from top of SDRAM */
	movl	%eax, %esp

	/* Re-enter U-Boot by calling board_init_f_r() */
	call	board_init_f_r

die:
	hlt
	jmp	die
	hlt
