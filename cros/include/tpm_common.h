/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2021 Google LLC
 */

#ifndef CROS_TPM_COMMON_H
#define CROS_TPM_COMMON_H

#include <cros/vboot.h>

int vboot_extend_pcrs(struct vboot_info *vboot);

/* Start of the root of trust */
int vboot_setup_tpm(struct vboot_info *vboot);

/* vboot_extend_pcr function for vb2 context */
vb2_error_t vboot_extend_pcr(struct vboot_info *vboot, int pcr,
			     enum vb2_pcr_digest which_digest);

#endif /* CROS_TPM_COMMON_H */
