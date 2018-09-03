/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Taken from coreboot file payloads/libpayload/include/archive.h
 *
 * Copyright 2018 Google LLC
 * written by Daisuke Nojiri <dnojiri@chromium.org>
 */

#ifndef __CROS_CB_ARCHIVE_H
#define __CROS_CB_ARCHIVE_H

/*
 * Archive file layout:
 *
 *  +----------------------------------+
 *  |           root header            |
 *  +----------------------------------+
 *  |         file_header[0]           |
 *  +----------------------------------+
 *  |         file_header[1]           |
 *  +----------------------------------+
 *  |              ...                 |
 *  +----------------------------------+
 *  |         file_header[count-1]     |
 *  +----------------------------------+
 *  |         file(0) content          |
 *  +----------------------------------+
 *  |         file(1) content          |
 *  +----------------------------------+
 *  |              ...                 |
 *  +----------------------------------+
 *  |         file(count-1) content    |
 *  +----------------------------------+
 */

#define VERSION		0
#define CBAR_MAGIC	"CBAR"
#define NAME_LENGTH	32

/**
 * struct directory - Root header
 *
 * @magic: Magic number (CBAR_MAGIC)
 * @version: version of the header, little endian
 * @size: total size of archive, little endian
 * @count: number of files, little endian
 */
struct directory {
	char magic[4];
	u32 version;
	u32 size;
	u32 count;
};

/**
 * struct dentry - File header
 *
 * @name: file name, nul-terminated if shorter than NAME_LENGTH
 * offset: file offset from the root header, little endian
 * @size: file size, little endian
 */
struct dentry {
	char name[NAME_LENGTH];
	u32 offset;
	u32 size;
};

/**
 * get_first_dentry() - Get the first file in a directory
 *
 * @dir: Directory to check
 * @return pointer to the first file
 */
static inline struct dentry *get_first_dentry(const struct directory *dir)
{
	return (struct dentry *)(dir + 1);
}

/**
 * get_first_offset() - Get the offset of the first file
 *
 * @dir: Directory to check
 * @return offset of the first file, in bytes
 */
static inline u32 get_first_offset(const struct directory *dir)
{
	return sizeof(struct directory) + sizeof(struct dentry) * dir->count;
}

#endif /* __CROS_CB_ARCHIVE_H */
