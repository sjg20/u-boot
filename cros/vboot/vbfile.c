// SPDX-License-Identifier: GPL-2.0+
/*
 * Interface for accessing files in SPI flash
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <cbfs.h>
#include <log.h>
#include <cros/cb_helper.h>
#include <cros/cros_ofnode.h>
#include <cros/fwstore.h>
#include <cros/vbfile.h>
#include <cros/vboot.h>

int vbfile_load(struct vboot_info *vboot, const char *name, struct abuf *buf)
{
	int ret;

	if (!vboot_from_cb(vboot)) {
		struct fmap_entry entry;

		ret = cros_ofnode_find_locale(name, &entry);
		if (ret)
			return log_msg_ret("find", ret);

		ret = fwstore_load_image(vboot->fwstore, &entry, buf);
		if (ret)
			return log_msg_ret("read", ret);
	} else {
		const struct cbfs_cachenode *file;

		file = cbfs_find_file(vboot->cbfs, name);
		if (!file) {
			log_err("Cannot find file '%s'\n", name);
			return log_msg_ret("cfind", -ENOENT);
		}
		if (file->comp_algo == CBFS_COMPRESS_NONE) {
			abuf_set(buf, file->data, file->data_length);
		} else {
			enum fmap_compress_t compress_algo;
			struct abuf in;

			compress_algo = cb_conv_compress_type(file->comp_algo);
			if (!abuf_realloc(buf, file->decomp_size))
				return log_msg_ret("lzma", -ENOMEM);

			abuf_init_set(&in, file->data, file->data_length);
			ret = fwstore_decomp_with_algo(compress_algo, &in, buf,
						       true);
			abuf_uninit(&in);
		}

		return 0;
	}

	return 0;
}

int vbfile_section_load(struct vboot_info *vboot, const char *section,
			const char *name, struct abuf *buf)
{
	/* TODO(sjg@chromium.org): Implement this, needed for altfw support */
	return -ENOSYS;
}
