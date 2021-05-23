/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common features for sandbox TPM1 and TPM2 implementations
 *
 * Copyright 2021 Google LLC
 */

#ifndef __TPM_SANDBOX_COMMON_H
#define __TPM_SANDBOX_COMMON_H

/*
 * These numbers derive from adding the sizes of command fields as shown in
 * the TPM commands manual.
 */
#define TPM_HDR_LEN	10

/* These are the different non-volatile spaces that we emulate */
enum sandbox_nv_space {
	NV_SEQ_ENABLE_LOCKING,
	NV_SEQ_GLOBAL_LOCK,
	NV_SEQ_FIRMWARE,
	NV_SEQ_KERNEL,
	NV_SEQ_BACKUP,
	NV_SEQ_FWMP,
	NV_SEQ_REC_HASH,

	NV_SEQ_COUNT,
};

struct __packed rollback_space_kernel {
	/* Struct version, for backwards compatibility */
	uint8_t struct_version;
	/* Unique ID to detect space redefinition */
	uint32_t uid;
	/* Kernel versions */
	uint32_t kernel_versions;
	/* Reserved for future expansion */
	uint8_t reserved[3];
	/* Checksum (v2 and later only) */
	uint8_t crc8;
};

int sb_tpm_index_to_seq(uint index);

void sb_tpm_read_data(struct udevice *tpm, enum sandbox_nv_space seq,
		      u8 *recvbuf, int data_ofs);

#endif

