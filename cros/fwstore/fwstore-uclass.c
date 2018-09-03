// SPDX-License-Identifier: GPL-2.0+
/*
 * Interface for accessing the firmware image in storage (e.g. SPI flash)
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <abuf.h>
#include <cbfs.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <cros/fwstore.h>
#include <dm/device-internal.h>
#include <lzma/LzmaTypes.h>
#include <lzma/LzmaDec.h>
#include <lzma/LzmaTools.h>
#include <u-boot/lz4.h>

int cros_fwstore_read(struct udevice *dev, int offset, int count, void *buf)
{
	struct cros_fwstore_ops *ops = cros_fwstore_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, offset, count, buf);
}

int cros_fwstore_read_entry_raw(struct udevice *dev, struct fmap_entry *entry,
				void *buf, int maxlen)
{
	int ret;

	if (entry->length > maxlen)
		return log_msg_ret("spc", -ENOSPC);
	if (!entry->length)
		return log_msg_ret("missing", -ENOENT);

	ret = cros_fwstore_read(dev, entry->offset, entry->length, buf);
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

int cros_fwstore_read_entry(struct udevice *dev, struct fmap_entry *entry,
			    struct abuf *buf)
{
	int ret;

	if (!entry->length)
		return log_msg_ret("missing", -ENOENT);
	if (!abuf_realloc(buf, entry->length))
		return log_msg_ret("mem", -ENOENT);

	ret = cros_fwstore_read(dev, entry->offset, entry->length,
				abuf_data(buf));
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

int cros_fwstore_mmap(struct udevice *dev, uint offset, uint size,
		      ulong *addrp)
{
	struct cros_fwstore_ops *ops = cros_fwstore_get_ops(dev);

	if (!ops->mmap)
		return -ENOSYS;

	return ops->mmap(dev, offset, size, addrp);
}

int fwstore_decomp_with_algo(enum fmap_compress_t algo, struct abuf *in,
			     struct abuf *out, bool is_cbfs)
{
	const size_t size = abuf_size(in);
	const void *indata;
	size_t comp_len;
	int ret;

	indata = abuf_data(in);
	if (!is_cbfs) {
		if (size < sizeof(u32))
			return log_msg_ret("sz", -ETOOSMALL);
		comp_len = *(u32 *)indata;
		if (comp_len > size - sizeof(u32)) {
			log_warning("comp_len=%zx, size=%zx\n", comp_len, size);
			return log_msg_ret("norm", -EOVERFLOW);
		}
		/* skip uncompressed size */
		indata += sizeof(u32);
	} else {
		comp_len = size;
	}

	log_debug("Decompress algo %d length=%zx, unc=%zx, size=%zx, data=%p\n",
		  algo, size, abuf_size(out), size, abuf_data(in));
	log_buffer(UCLASS_CROS_FWSTORE, LOGL_DEBUG, 0, abuf_data(in), 1, 0x80,
		   0);
	switch (algo) {
	case FMAP_COMPRESS_LZMA: {
		if (!CONFIG_IS_ENABLED(LZMA))
			return log_msg_ret("lzma", -ENOTSUPP);
		SizeT inout_size = abuf_size(out);

		ret = lzmaBuffToBuffDecompress(abuf_data(out), &inout_size,
					       indata, comp_len);
		if (ret)
			return log_msg_ret("lzmad", ret);
		if (!abuf_realloc(out, inout_size))
			return log_msg_ret("lz4m", -ENOMEM);
		break;
	}
	case FMAP_COMPRESS_LZ4: {
		size_t out_size;

		if (!CONFIG_IS_ENABLED(LZ4))
			return log_msg_ret("lz4", -ENOTSUPP);
		ret = ulz4fn(indata, comp_len, abuf_data(out), &out_size);
		if (ret)
			return log_msg_ret("lz4d", ret);
		if (!abuf_realloc(out, out_size))
			return log_msg_ret("lz4m", -ENOMEM);
		break;
	}
	default:
		return log_msg_ret("unknown", -EPROTONOSUPPORT);
	}

	return 0;
}

int fwstore_read_decomp(struct udevice *dev, struct fmap_entry *entry,
			struct abuf *buf)
{
	struct cros_fwstore_ops *ops = cros_fwstore_get_ops(dev);
	struct abuf readbuf;
	int ret;

	if (!ops->read)
		return log_ret(-ENOSYS);

	/* Read the data into the buffer */
	if (entry->compress_algo == FMAP_COMPRESS_NONE) {
		abuf_init_set(&readbuf, abuf_data(buf), abuf_size(buf));
	} else {
		if (abuf_size(buf) < entry->unc_length)
			return log_ret(-ENOSPC);
		abuf_init_set(&readbuf, abuf_data(buf) +
			ALIGN(abuf_size(buf) - entry->unc_length, 4),
			entry->unc_length);

	}
	ret = ops->read(dev, entry->offset, entry->length, abuf_data(&readbuf));
	if (ret) {
		abuf_uninit(&readbuf);
		return log_msg_ret("read", ret);
	}

	if (entry->compress_algo != FMAP_COMPRESS_NONE) {
		ret = fwstore_decomp_with_algo(entry->compress_algo, &readbuf,
					       buf, false);
		abuf_uninit(&readbuf);
		if (ret)
			return log_msg_ret("decomp", ret);
	} else {
		abuf_uninit(&readbuf);
	}

	return 0;
}

int fwstore_get_reader_dev(struct udevice *fwstore, int offset, int size,
			   struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	if (device_find_first_inactive_child(fwstore, UCLASS_MISC, &dev)) {
		ret = device_bind(fwstore, DM_DRIVER_GET(fwstore_reader),
				  "fwstore_reader", 0, ofnode_null(), &dev);
		if (ret)
			return log_msg_ret("bind failed", ret);
	}
	fwstore_reader_setup(dev, offset, size);
	ret = device_probe(dev);
	if (ret)
		return ret;
	*devp = dev;

	return 0;
}

static int fwstore_decomp_data(struct fmap_entry *entry, struct abuf *in,
			       bool is_cbfs, struct abuf *out)
{
	int ret;

	if (!abuf_realloc(out, entry->unc_length + 100))
		return log_msg_ret("allocate decomp buf", -ENOMEM);
	log_debug("Decompress algo %d length=%x, buf_size=%zx\n",
		  entry->compress_algo, entry->length, abuf_size(out));
	log_buffer(UCLASS_CROS_FWSTORE, LOGL_DEBUG, 0, abuf_data(in), 1, 0x80,
		   0);

	ret = fwstore_decomp_with_algo(entry->compress_algo, in, out, is_cbfs);
	if (ret)
		return log_msg_ret("decomp", ret);

	return 0;
}

int fwstore_load_image(struct udevice *dev, struct fmap_entry *entry,
		       struct abuf *buf)
{
	struct abuf tmp;
	bool is_cbfs;
	ulong addr;
	int ret;

	if (!entry->length)
		return log_msg_ret("no image", -ENOENT);

	/* Get a pointer to the data in 'data' */
	abuf_init(&tmp);
	is_cbfs = CONFIG_IS_ENABLED(CHROMEOS_COREBOOT) && entry->cbfs_node;
	if (is_cbfs) {
		log_info("load entry from CBFS %s\n", entry->cbfs_node ?
			 entry->cbfs_node->name : "(null)");
		if (!entry->cbfs_node)
			return log_msg_ret("cbfs", -ENOENT);
		abuf_set(&tmp, entry->cbfs_node->data,
			 entry->cbfs_node->data_length);
	} else {
		/*
		 * Try mapping first as it avoids the allocation and might be
		 * faster
		 */
		log_info("load entry at %x, size %x\n", entry->offset,
			 entry->length);
		ret = fwstore_entry_mmap(dev, entry, &addr);
		if (!ret) {
			abuf_map_sysmem(&tmp, addr, entry->length);
			log_info("- mapped to %p\n", abuf_data(&tmp));
		} else {
			if (!abuf_realloc(&tmp, entry->length))
				return log_msg_ret("allocate space for image",
						   -ENOMEM);
			log_info("- loading into buffer at %p\n",
				 abuf_data(&tmp));

			ret = cros_fwstore_read(dev, entry->offset,
						entry->length, abuf_data(&tmp));
			if (ret) {
				ret = log_msg_ret("read image", ret);
				goto err;
			}
		}
	}

	if (entry->compress_algo != FMAP_COMPRESS_NONE) {
		ret = fwstore_decomp_data(entry, &tmp, is_cbfs, buf);
		if (ret) {
			ret = log_msg_ret("decomp", ret);
			goto err;
		}
	} else {
		abuf_set(buf, abuf_data(&tmp), entry->length);
	}
	abuf_uninit(&tmp);

	return 0;

err:
	abuf_uninit(&tmp);
	return ret;
}

int fwstore_entry_mmap(struct udevice *dev, struct fmap_entry *entry,
		       ulong *addrp)
{
	return cros_fwstore_mmap(dev, entry->offset, entry->length, addrp);
}

UCLASS_DRIVER(cros_fwstore) = {
	.id		= UCLASS_CROS_FWSTORE,
	.name		= "cros_fwstore",
};
