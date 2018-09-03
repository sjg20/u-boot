/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Interface for access the firmware image in storage (e.g. SPI flash)
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __CROS_FWSTORE_H_
#define __CROS_FWSTORE_H_

#include <dm/of_extra.h>

struct abuf;

/**
 * These read or write [count] bytes starting from [offset] of storage into or
 * from the [buf].
 *
 * @return 0 if it succeeds, non-zero if it fails
 */
struct cros_fwstore_ops {
	/**
	 * read() - read data
	 *
	 * @dev:	Device to read from
	 * @offset:	Offset within device to read from in bytes
	 * @count:	Number of bytes to read
	 * @buf:	Buffer to place data
	 * @return 0 if OK, -ve on error
	 */
	int (*read)(struct udevice *dev, ulong offset, ulong count, void *buf);

	/**
	 * write() - write data
	 *
	 * @dev:	Device to write to
	 * @offset:	Offset within device to write to in bytes
	 * @count:	Number of bytes to write
	 * @buf:	Buffer containg data to write
	 * @return 0 if OK, -ve on error
	 */
	int (*write)(struct udevice *dev, ulong offset, ulong count, void *buf);

	/**
	 * sw_wp_enabled() - see if software write protect is enabled
	 *
	 * @dev:	Device to check
	 * @return	1 if sw wp is enabled, 0 if disabled, -ve on error
	 */
	int (*sw_wp_enabled)(struct udevice *dev);

	/**
	 * mmap() - Find the memory-mapped address of a fwstore offset
	 *
	 * @dev:	Device to map
	 * @offset:	Offset within device to map
	 * @count:	Number of bytes to map
	 * @addrp:	Returns map address
	 * @return 0 if OK, -ve on error
	 */
	int (*mmap)(struct udevice *dev, uint offset, uint size, ulong *addrp);

};

#define cros_fwstore_get_ops(dev) \
		((struct cros_fwstore_ops *)(dev)->driver->ops)

/**
 * cros_fwstore_read() - read data
 *
 * @dev:	Device to read from
 * @offset:	Offset within device to read from in bytes
 * @count:	Number of bytes to read
 * @buf:	Buffer to place data
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_read(struct udevice *dev, int offset, int count, void *buf);

/**
 * cros_fwstore_write() - write data
 *
 * @dev:	Device to write to
 * @offset:	Offset within device to write to in bytes
 * @count:	Number of bytes to write
 * @buf:	Buffer containg data to write
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_write(struct udevice *dev, int offset, int count, void *buf);

/**
 * cros_fwstore_get_sw_write_prot() - see if software write protect is enabled
 *
 * @dev:	Device to check
 * @return	1 if sw wp is enabled, 0 if disabled, -ve on error
 */
int cros_fwstore_get_sw_write_prot(struct udevice *dev);

/**
 * cros_fwstore_mmap() - Find the memory-mapped address of a fwstore offset
 *
 * @dev:	Device to map
 * @offset:	Offset within device to map
 * @count:	Number of bytes to map
 * @addrp:	Returns map address
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_mmap(struct udevice *dev, uint offset, uint size,
		      ulong *addrp);

/**
 * fwstore_reader_setup() - Set up an existing reader for SPI flash
 *
 * This sets the platform data for the reader device so that it can operate
 * correctly. The device should be inactive. It is not probed by this function.
 *
 * @dev: Device to set up
 * @offset: Start offset in flash to allow the device to access
 * @size: Size of region to allow the device to access
 */
void fwstore_reader_setup(struct udevice *dev, int offset, int size);

/**
 * fwstore_reader_restrict() - Restrict the boundaries of a device
 *
 * This reduces the size of an fwstore reader to extend only between the new
 * offset and size, relative to the existing size of the device.
 *
 * @dev: reader device to update
 * @offset: Offset within the parent device to start from
 * @size: Number of bytes to present as available
 * @return 0 if OK, -EINVAL if the postion is out of range, -ve on error
 */
int fwstore_reader_restrict(struct udevice *dev, int offset, int size);

/**
 * fwstore_reader_size() - Get the size of a reader
 *
 * @dev: reader device to check
 * @return reader size in bytes
 */
int fwstore_reader_size(struct udevice *dev);

/**
 * fwstore_get_reader_dev() - Create a reader from an fwstore device
 *
 * fwstore: Device to read from (UCLASS_CROS_FWSTORE)
 * @offset: Start offset within device
 * @size: Size within device (starting at @offset)
 * @devp: Returns new reader device (UCLASS_MISC)
 * @return 0 if OK, -ve on error
 */
int fwstore_get_reader_dev(struct udevice *fwstore, int offset, int size,
			   struct udevice **devp);

/**
 * fwstore_load_image() - Allocate and load an image from the firmware store
 *
 * This allocates memory for the image and returns a pointer to it.
 *
 * @dev: Device to load from
 * @entry:	Flash entry to load (provides offset, size, compression,
 *		uncompressed size)
 * @buf:	Returns the data (caller must init the buf before calling this
 *		function and is responsible for calling abuf_uninit()
 *		afterwards, regardless of error
 * @return 0 if OK, -ENOENT if the image has a zero size, -ENOMEM if there is
 *	not enough memory for the buffer, other error on read failre
 */
int fwstore_load_image(struct udevice *dev, struct fmap_entry *entry,
		       struct abuf *buf);

/**
 * fwstore_read_decomp: Read and decompress data into a buffer
 *
 * @dev: Device to read from (UCLASS_FWSTORE)
 * @entry: Flashmap entry to read from (indicates position, compression, etc.)
 * @buf: Buffer to read into (must be inited by caller)
 * @buf_size: Size of buffer
 * @return 0 if OK, -ve on error
 */
int fwstore_read_decomp(struct udevice *dev, struct fmap_entry *entry,
			struct abuf *buf);

/**
 * fwstore_decomp_with_algo() - Decompress some data
 *
 * @algo: Compression algorithm to use
 * @in: Compressed data to decompress
 * @out: Output buffer for decompressed data (must be inited by caller)
 * @is_cbfs: true if this is a CBFS region, rather than binman. Binman adds the
 * u32 size of the compressed data to the start of @data, with @is_cbfs this is
 * not expected
 * @return 0 if OK, -ETOOSMALL is @size is too small to contain useful data,
 * @EOVERFLOW if the compressed data appears to extend beyond @size (only
 * detected when @is_cbfs is false), -EEPROTONOSUPPORT if the compression
 * algorithm is unknown, -ENOTSUPP if it is not supported (CONFIG option),
 * other -ve value if the decompression fails due to corruption or unsupported
 * compression options within the compressed stream.
 */
int fwstore_decomp_with_algo(enum fmap_compress_t algo, struct abuf *in,
			     struct abuf *out, bool is_cbfs);

/**
 * cros_fwstore_read_entry() - read data
 *
 * This does a simple read of data, without support for compression
 *
 * @dev:	Device to read from
 * @entry:	Flash entry to load (provides offset, size)
 * @buf:	Buffer to place data
 * @maxlen:	Length of buffer
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_read_entry_raw(struct udevice *dev, struct fmap_entry *entry,
				void *buf, int maxlen);

/**
 * cros_fwstore_read_entry() - read data into an abuf
 *
 * This does a simple read of data, without support for compression
 *
 * @dev:	Device to read from
 * @entry:	Flash entry to load (provides offset, size)
 * @buf:	abuf to place data (caller must init the buf before calling this
 *		function and is responsible for calling abuf_uninit()
 *		afterwards, regardless of error
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_read_entry(struct udevice *dev, struct fmap_entry *entry,
			    struct abuf *buf);

/**
 * fwstore_entry_mmap() - Find the memory-mapped address of an entry
 *
 * @dev:	Device to map
 * @entry:	Flash entry to map
 * @addrp:	Returns map address
 * @return 0 if OK, -ENOSYS if not supported, other -ve on mapping error
 */
int fwstore_entry_mmap(struct udevice *dev, struct fmap_entry *entry,
		       ulong *addrp);

#endif /* __CROS_FWSTORE_H_ */
