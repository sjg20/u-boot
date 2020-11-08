/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Bindings for nvdata-uclass
 *
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _DT_BINDINGS_CROS_NVDATA_H
#define _DT_BINDINGS_CROS_NVDATA_H

/* enum cros_nvdata_type */

#define CROS_NV_DATA		0	/* Standard data (can be lost) */
#define CROS_NV_SECDATA		1	/* Secure data (e.g. stored in TPM) */
#define CROS_NV_SECDATAK	2	/* Secure data for kernel */
#define CROS_NV_REC_HASH	3	/* Recovery-mode hash */
#define CROS_NV_VSTORE		4	/* Verified boot storage slot 0 */

#endif /* _DT_BINDINGS_CROS_NVDATA_H */
