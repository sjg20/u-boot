// SPDX-License-Identifier:     BSD-3-Clause
/*
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 *
 * Taken from coreboot file antirollback.h *
 * Copyright 2021 Google LLC
 */

#ifndef CROS_ANTIROLLBACK_H_
#define CROS_ANTIROLLBACK_H_

#include <vb2_sha.h>

enum cros_nvdata_type;
struct vb2_context;
enum vb2_pcr_digest;
struct vboot_info;

/* TPM NVRAM location indices. */
#define FIRMWARE_NV_INDEX               0x1007
#define KERNEL_NV_INDEX                 0x1008
/* 0x1009 used to be used as a backup space. Think of conflicts if you
 * want to use 0x1009 for something else. */
#define BACKUP_NV_INDEX                 0x1009
#define FWMP_NV_INDEX                   0x100a
/* 0x100b: Hash of MRC_CACHE training data for recovery boot */
#define MRC_REC_HASH_NV_INDEX           0x100b
/* 0x100c: OOBE autoconfig public key hashes */
/* 0x100d: Hash of MRC_CACHE training data for non-recovery boot */
#define MRC_RW_HASH_NV_INDEX            0x100d
#define HASH_NV_SIZE                    VB2_SHA256_DIGEST_SIZE

/* Structure definitions for TPM spaces */

/* Flags for firmware space */

/*
 * Last boot was developer mode.  TPM ownership is cleared when transitioning
 * to/from developer mode.
 */
#define FLAG_LAST_BOOT_DEVELOPER 0x01

/* All functions return 0 if successful, non-zero if error */
int antirollback_read_space_firmware(struct vboot_info *vboot);

/**
 * Write may be called if the versions change.
 */
int antirollback_write_space_firmware(const struct vboot_info *vboot);

/**
 * Read and write kernel space in TPM.
 */
int antirollback_read_space_kernel(const struct vboot_info *vboot);
int antirollback_write_space_kernel(const struct vboot_info *vboot);

/**
 * Lock must be called.
 */
int antirollback_lock_space_firmware(void);

/*
 * Read MRC hash data from TPM.
 * @param index index into TPM NVRAM where hash is stored The index
 *              can be set to either MRC_REC_HASH_NV_INDEX or
 *              MRC_RW_HASH_NV_INDEX depending upon whether we are
 *              booting in recovery or normal mode.
 * @param data  pointer to buffer where hash from TPM read into
 * @param size  size of buffer
 */
int antirollback_read_space_mrc_hash(enum cros_nvdata_type type, uint8_t *data,
				     u32 size);

/*
 * Write new hash data to MRC space in TPM.\
 * @param index index into TPM NVRAM where hash is stored The index
 *              can be set to either MRC_REC_HASH_NV_INDEX or
 *              MRC_RW_HASH_NV_INDEX depending upon whether we are
 *              booting in recovery or normal mode.
 * @param data  pointer to buffer of hash value to be written
 * @param size  size of buffer
*/
int antirollback_write_space_mrc_hash(enum cros_nvdata_type type,
				      const uint8_t *data, u32 size);
/*
 * Lock down MRC hash space in TPM.
 * @param index index into TPM NVRAM where hash is stored The index
 *              can be set to either MRC_REC_HASH_NV_INDEX or
 *              MRC_RW_HASH_NV_INDEX depending upon whether we are
 *              booting in recovery or normal mode.
*/
int antirollback_lock_space_mrc_hash(enum cros_nvdata_type type);

#endif  /* CROS_ANTIROLLBACK_H_ */
