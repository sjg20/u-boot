// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Google, Inc
 *
 * Simple program to create some binman symbols. This is used by binman tests.
 */

typedef unsigned long ulong;

#include <linux/kconfig.h>
#include <binman_sym.h>

DECLARE_BINMAN_MAGIC_SYM;

unsigned long val1;
binman_sym_declare(unsigned long, inset, offset);
unsigned long val2;
