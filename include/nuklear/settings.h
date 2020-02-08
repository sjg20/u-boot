/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Settings for Nuklear
 *
 * Copyright Google LLC 2019
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __NUKCLEAR_SETTINGS_H
#define __NUKCLEAR_SETTINGS_H

#include <common.h>

#define NK_ASSERT(expr) (void)(expr)

#define NK_INCLUDE_FIXED_TYPES
//#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
/*
#define NK_INCLUDE_DEFAULT_ALLOCATOR
*/
#define NK_IMPLEMENTATION
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_SOFTWARE_FONT
#define NK_INCLUDE_COMMAND_USERDATA

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ASSERT assert

#endif
