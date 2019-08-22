/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019 Google LLC
 */

#ifndef __IRQ_LEGACY_H
#define __IRQ_LEGACY_H

int interrupt_init(void);
void timer_interrupt(struct pt_regs *regs);
void external_interrupt(struct pt_regs *regs);
void irq_install_handler (int vec, interrupt_handler_t *handler, void *arg);
void irq_free_handler(int vec);
void reset_timer(void);

#endif
