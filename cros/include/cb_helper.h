/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Helper functions used when booting from coreboot
 *
 * Copyright 2021 Google LLC
 */

#ifndef __CROS_CB_HELPER_H
#define __CROS_CB_HELPER_H

#include <dm/of_extra.h>

struct fmap_section;
struct memwipe;
struct sysinfo_t;
struct vb2_context;
struct vboot_handoff;

/**
 * cb_conv_compress_type() - Convert a CBFS compression algorithm tag to FMAP
 *
 * @cbfs_comp_algo: CBFS compression tag, e.g. CBFS_COMPRESS_LZMA
 * @return corresponding FMAP compression tag, or FMAP_COMPRESS_UNKNOWN if not
 *	known
 */
enum fmap_compress_t cb_conv_compress_type(uint cbfs_comp_algo);

/**
 * cb_scan_cbfs() - Scan a CBFS filesystem
 *
 * @vboot: Pointer to vboot structure
 * @offset: Offset of CBFS in fwstore
 * @size: Size of CBFS in bytes
 * @cbfsp: Returns pointer to the CBFS cache on success
 * @return 0 on sucess, -ve on failure
 */
int cb_scan_cbfs(struct vboot_info *vboot, uint offset, uint size,
		 struct cbfs_priv **cbfsp);

/**
 * cb_fmap_read() - Read the flashmap
 *
 * When booting from coreboot, the FMAP provides information about the location
 * of the various pieces needed by verified boot. This function finds the FMAP
 * and parses it to find the things that are needed
 *
 * @vboot: vboot context
 * @return 0 if OK, -ve on error
 */
int cb_fmap_read(struct vboot_info *vboot);

/**
 * cb_scan_files() - Scan the CBFS for useful files
 *
 * With coreboot the contents are not accessible in a central directory, but via
 * scanning the filesystem file by file. Scan to find files that are needed for
 * vboot, such as the EC binary, storing the CBFS pointer in each case. This
 * allows easy access later, via the fmap_section struct.
 *
 * @cbfs: Pointer to CBFS cache
 * @section: Sectoin data to fill in
 * @return 0 if OK, -EPROTONOSUPPORT if an unknown compresion algorithm is used
 */
int cb_scan_files(struct cbfs_priv *cbfs, struct fmap_section *section);

/**
 * cb_setup_unused_memory() - find memory to clear
 *
 * Checks the coreboot tables to figure out what memory should be cleared
 * @vboot: vboot context
 * @wipe: Information about memory to wipe
 * @return 0 if OK, -EPERM if coreboot table cannot be found (fatal error)
 */
int cb_setup_unused_memory(struct vboot_info *vboot, struct memwipe *wipe);

/**
 * cb_read_model() - Read the model name from the coreboot tables
 *
 * This looks up the model name in the mainboard SMBIOS tables.
 *
 * @sysinfo: sysinfo parsed from coreboot tables
 * @return model name, or NULL if not found
 */
const char *cb_read_model(const struct sysinfo_t *sysinfo);

/**
 * cb_vboot_rw_init() - setup the read/write vboot when booting from coreboot
 *
 * This prints the model we are booting on and sets up the vboot context based
 * on the handoff info from coreboot
 *
 * @vboot: vboot context
 * @return 0 if OK, -ENOENT if no handoff info, other -ve on other error
 */
int cb_vboot_rw_init(struct vboot_info *vboot, struct vb2_context **ctxp);

/**
 * cb_get_vboot_handoff() - Obtain the vboot handout pointer from coreboot
 *
 * @return vboot handoff pointer, or NULL if not available
 */
struct vboot_handoff *cb_get_vboot_handoff(void);

/**
 * cb_setup_flashmap() - Locate the flashmap and UI bits from coreboot tables
 *
 * When booting from coreboot we must look up the sysinfo information parsed
 * from the coreboot tables to find which CBFS is being used. We also need to
 * parse the FMAP. This function handles this, equivalent to parsing the
 * binman flashmap when booting bare-metal.
 *
 * @vboot: vboot context
 */
int cb_setup_flashmap(struct vboot_info *vboot);

#endif /* __CROS_CB_HELPER_H */
