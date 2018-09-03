/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Interface for accessing files in SPI flash
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __CROS_VBFILE_H
#define __CROS_VBFILE_H

struct vboot_info;

/**
 * vbfile_load() - load a file from the firmware store
 *
 * @vboot: Vboot context
 * @name: Filename to load, normally a locale filename
 *	If vboot_from_cb(vboot), i.e. booting from coreboot, this supports any
 *	filename available in the read-only CBFS
 * @buf: abuf to place data (caller must init the buf before calling this
 *	function and is responsible for calling abuf_uninit()
 *	afterwards, regardless of error
 * @return 0 if OK, -EINVAL if no locales could be found, -ENOENT if the
 *	requested file was not found, -ENOMEM if not enough memory to allocate
 *	the file, other -ve value on other errorS
 */
int vbfile_load(struct vboot_info *vboot, const char *name, struct abuf *buf);

int vbfile_section_load(struct vboot_info *vboot, const char *section,
			const char *name, struct abuf *buf);

#endif /* __CROS_VBFILE_H */
