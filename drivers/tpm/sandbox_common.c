// SPDX-License-Identifier: GPL-2.0+
/*
 * Common features for sandbox TPM1 and TPM2 implementations
 *
 * Copyright 2021 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <tpm-v1.h>
#include <tpm-v2.h>
#include <asm/unaligned.h>
#include <u-boot/crc.h>
#include "sandbox_common.h"

#define TPM_ERR_CODE_OFS	(2 + 4)		/* after tag and size */

/* Kernel TPM space - KERNEL_NV_INDEX, locked with physical presence */
#define ROLLBACK_SPACE_KERNEL_VERSION	2
#define ROLLBACK_SPACE_KERNEL_UID	0x4752574C  /* 'GRWL' */

struct rollback_space_kernel rollback_space_kernel;

int sb_tpm_index_to_seq(u32 index)
{
	index &= ~HR_NV_INDEX;
	switch (index) {
	case FIRMWARE_NV_INDEX:
		return NV_SEQ_FIRMWARE;
	case KERNEL_NV_INDEX:
		return NV_SEQ_KERNEL;
	case BACKUP_NV_INDEX:
		return NV_SEQ_BACKUP;
	case FWMP_NV_INDEX:
		return NV_SEQ_FWMP;
	case MRC_REC_HASH_NV_INDEX:
		return NV_SEQ_REC_HASH;
	case 0:
		return NV_SEQ_GLOBAL_LOCK;
	case TPM_NV_INDEX_LOCK:
		return NV_SEQ_ENABLE_LOCKING;
	}

	printf("Invalid nv index %#x\n", index);
	return -1;
}

void sb_tpm_read_data(const struct nvdata_state nvdata[NV_SEQ_COUNT],
		      enum sandbox_nv_space seq, u8 *recvbuf, int data_ofs,
		      int length)
{
	u8 *data = recvbuf + data_ofs;

	if (seq == NV_SEQ_KERNEL) {
		struct rollback_space_kernel rsk;

		memset(&rsk, 0, sizeof(struct rollback_space_kernel));
		rsk.struct_version = 2;
		rsk.uid = ROLLBACK_SPACE_KERNEL_UID;
		rsk.crc8 = crc8(0, (unsigned char *)&rsk,
				offsetof(struct rollback_space_kernel,
					 crc8));
		memcpy(data, &rsk, sizeof(rsk));
	} else if (!nvdata[seq].present) {
		put_unaligned_be32(TPM_BADINDEX,
				   recvbuf + TPM_ERR_CODE_OFS);
	} else {
		memcpy(data, &nvdata[seq].data, length);
	}
}
