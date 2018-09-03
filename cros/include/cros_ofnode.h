/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Device-tree node-handling code
 *
 * Copyright 2018 Google LLC
 */

#ifndef __CROS_OFNODE_H
#define __CROS_OFNODE_H

#include <dm/ofnode.h>
#include <dm/of_extra.h>

/**
 * enum cros_compress_t - compression types supported
 *
 * @CROS_COMPRESS_NONE: Not compressed
 * @CROS_COMPRESS_LZO: Lempel–Ziv–Oberhumer (LZO) compression
 */
enum cros_compress_t {
	CROS_COMPRESS_NONE,
	CROS_COMPRESS_LZO,
};

/* Structures to hold Chromium OS specific configuration from the FMAP */

/* struct fmap_entry is now defined in fdtdec.h */
#include <fdtdec.h>

/**
 * List of EC images available
 *
 * @EC_MAIN: Main Chrome OS EC
 * @EC_PD: USB Power Delivery controller
 */
enum ec_index_t {
	EC_MAIN,
	EC_PD,

	EC_COUNT
};

/** FMAP information for read-only and read-write EC images */
struct fmap_ec {
	struct fmap_entry ro;
	struct fmap_entry rw;
};

/**
 * struct fmap_section - information about a section
 *
 * This holds information about all the binaries in a particular part of the
 * image, such as read-only, RW-A, RW-B.
 *
 * @all: Size and position of the entire section
 * @spl: Information about SPL
 * @boot: Information about U-Boot
 * @vblock: Information about  the vblock
 * @firmware_id: Information about the firmware ID string
 * @ec: Information about each EC
 * @gbb: Information about the Google Binary Block
 * @fmap: Information about the FMAP (Flash Map)
 * @spl_rec: Information about SPL recovery
 * @boot_rec: Information about U-Boot recovery
 */
struct fmap_section {
	struct fmap_entry all;
	struct fmap_entry spl;
	struct fmap_entry boot;
	struct fmap_entry vblock;
	struct fmap_entry firmware_id;

	/* EC RW binary, and RO binary if present */
	struct fmap_ec ec[EC_COUNT];

	struct fmap_entry gbb;
	struct fmap_entry fmap;

	/* U-Boot recovery */
	struct fmap_entry spl_rec;
	struct fmap_entry boot_rec;
};

/*
 * struct cros_fmap - Full FMAP as parsed from binman info
 *
 * Only sections that are used during booting are put here. More sections will
 * be added if required.
 *
 * @readonly: Information about the read-only section
 * @readwrite_a: Information about the read-write section A
 * @readwrite_b: Information about the read-write section B
 * @readwrite_devkey: Key for developer mode
 * @elog: Location of the ELOG (event log)
 * @flash_base: Base offset of the flash
 */
struct cros_fmap {
	struct fmap_section readonly;
	struct fmap_section readwrite_a;
	struct fmap_section readwrite_b;
	struct fmap_entry readwrite_devkey;
	struct fmap_entry elog;
	u32  flash_base;
};

/**
 * cros_ofnode_flashmap() - Decode Chromium OS-specific configuration from fdt
 *
 * @config: Returns decoded flashmap
 * @return 0 if OK, -ve on error
 */
int cros_ofnode_flashmap(struct cros_fmap *config);

/**
 * cros_ofnode_config_node() - Return the /chromeos-config ofnode
 *
 * @return ofnode found, of ofnode_null() if not found
 */
ofnode cros_ofnode_config_node(void);

/**
 * cros_ofnode_decode_region() - Decode a named region within a memory bank
 *
 * The properties are looked up in the /chromeos-config node
 *
 * See ofnode_decode_memory_region() for more details.
 *
 * @mem_type:	Type of memory to use, which is a name, such as "u-boot" or
 *		"kernel".
 * @suffix:	String to append to the memory/offset property names
 * @basep:	Returns base of region
 * @sizep:	Returns size of region
 * @return 0 if OK, -ve on error
 */
int cros_ofnode_decode_region(const char *mem_type, const char *suffix,
			      fdt_addr_t *basep, fdt_size_t *sizep);

/**
 * cros_ofnode_memory() - Returns information about memory for a given root
 *
 * @name:	Root name of alias to search for
 * @config:	structure to use to return information
 * @return 0 if OK, -ve on error
 */
int cros_ofnode_memory(const char *name, struct fdt_memory *config);

/**
 * cros_ofnode_find_locale() - Find the entry which contains a locale
 *
 * Locate the flashmap entry containing the Chromium OS locale information for
 * a given locale, used for the verified boot screens.
 *
 * @name: Name of locale filename to find (e.g. 'locale_en.bin')
 * @entry: Returns entry containing this locale
 * @return 0 if OK, -EINVAL if no locales could be found, -ENOENT if the
 *	requested locale was not found, other -ve error value on other error
 */
int cros_ofnode_find_locale(const char *name, struct fmap_entry *entry);

/**
 * cros_ofnode_dump_fmap() - Dump the position and side of the fmap regions
 *
 * This dumps out commonly used FMAP regions, showing their offset and size
 *
 * @config: FMAP config to dump
 */
void cros_ofnode_dump_fmap(struct cros_fmap *config);

#endif /* __CROS_OFNODE_H */
