/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Parsing of coreboot FMAP (flash map) structure
 * Taken from coreboot fmap.h
 *
 * Copyright 2018 Google LLC
 */

#ifndef __CROS_FMAP_H
#define __CROS_FMAP_H

#define FMAP_SIGNATURE		"__FMAP__"
#define FMAP_VER_MAJOR		1	/* this header's FMAP minor version */
#define FMAP_VER_MINOR		1	/* this header's FMAP minor version */

/* maximum length for strings, including null-terminator */
#define FMAP_STRLEN		32

struct cros_fmap;

enum fmap_flags {
	FMAP_AREA_STATIC	= 1 << 0,
	FMAP_AREA_COMPRESSED	= 1 << 1,
	FMAP_AREA_RO		= 1 << 2,
};

/* Mapping of volatile and static regions in firmware binary */
struct fmap_area {
	u32 offset;			/* offset relative to base */
	u32 size;			/* size in bytes */
	u8  name[FMAP_STRLEN];		/* descriptive name */
	u16 flags;			/* flags for this area */
}  __packed;

struct fmap {
	u8  signature[8];		/* "__FMAP__" (0x5F5F464D41505F5F) */
	u8  ver_major;			/* major version */
	u8  ver_minor;			/* minor version */
	u64 base;			/* address of the firmware binary */
	u32 size;			/* size of firmware binary in bytes */
	u8  name[FMAP_STRLEN];		/* name of this firmware binary */
	/* number of areas described by fmap_areas[] below */
	u16 nareas;
	struct fmap_area areas[];
} __packed;

#endif /* __CROS_FMAP_H */
