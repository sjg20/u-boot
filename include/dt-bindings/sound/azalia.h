/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Intel HDA audio codec config. This is a mechanicm to configure codecs when
 * using Intel HDA audio.
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __AZALIA_H
#define __AZALIA_H

#define AZALIA_PIN_CFG(codec, pin, val)			\
	(((codec) << 28) | ((pin) << 20) | (0x71c << 8)	\
		| ((val) & 0xff))			\
	(((codec) << 28) | ((pin) << 20) | (0x71d << 8)	\
		| (((val) >> 8) & 0xff))		\
	(((codec) << 28) | ((pin) << 20) | (0x71e << 8)	\
		| (((val) >> 16) & 0xff))		\
	(((codec) << 28) | ((pin) << 20) | (0x71f << 8)	\
		| (((val) >> 24) & 0xff))

#define AZALIA_SUBVENDOR(codec, val)		    \
	(((codec) << 28) | (0x01720 << 8) | ((val) & 0xff))	\
	(((codec) << 28) | (0x01721 << 8) | (((val) >> 8) & 0xff)) \
	(((codec) << 28) | (0x01722 << 8) | (((val) >> 16) & 0xff)) \
	(((codec) << 28) | (0x01723 << 8) | (((val) >> 24) & 0xff))

#endif /* __AZALIA_H */
