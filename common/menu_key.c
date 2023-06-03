// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2010-2011 Calxeda, Inc.
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 */

#include <common.h>
#include <cli.h>
#include <menu.h>

enum bootmenu_key bootmenu_conv_key(int ichar)
{
	enum bootmenu_key key;

	switch (ichar) {
	case '\n':
		/* enter key was pressed */
		key = BKEY_SELECT;
		break;
	case CTL_CH('c'):
	case '\e':
		/* ^C was pressed */
		key = BKEY_QUIT;
		break;
	case CTL_CH('p'):
		key = BKEY_UP;
		break;
	case CTL_CH('n'):
		key = BKEY_DOWN;
		break;
	case CTL_CH('s'):
		key = BKEY_SAVE;
		break;
	case '+':
		key = BKEY_PLUS;
		break;
	case '-':
		key = BKEY_MINUS;
		break;
	case ' ':
		key = BKEY_SPACE;
		break;
	default:
		key = BKEY_NONE;
		break;
	}

	return key;
}
