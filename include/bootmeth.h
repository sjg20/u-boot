/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootmeth_h
#define __bootmeth_h

struct bootflow;
struct bootflow_iter;
struct udevice;

/**
 * struct bootmeth_uc_plat - information the uclass keeps about each bootmeth
 *
 * @desc: A long description of the bootmeth
 */
struct bootmeth_uc_plat {
	const char *desc;
};

/** struct bootmeth_ops - Operations for boot methods */
struct bootmeth_ops {
	/**
	 * check_supported() - check if a bootmeth supports this bootflow
	 *
	 * This is optional. If not provided, the bootdev is assumed to be
	 * supported
	 *
	 * The bootmeth can check the bootdev (e.g. to make sure it is a
	 * network device) or the partition information. The following fields
	 * in @iter are available:
	 *
	 *   name, dev, state, part
	 *   max_part may be set if part != 0 (i.e. there is a valid partition
	 *      table). Otherwise max_part is 0
	 *   method is available but is the same as @dev
	 *   the partition has not yet been read, nor has the filesystem been
	 *   checked
	 *
	 * It may update only the flags in @iter
	 *
	 * @dev:	Bootmethod device to check against
	 * @iter:	On entry, provides bootdev, hwpart, part
	 * @return 0 if OK, -ENOTSUPP if this bootdev is not supported
	 */
	int (*check)(struct udevice *dev, struct bootflow_iter *iter);

	/**
	 * read_bootflow() - read a bootflow for a device
	 *
	 * @dev:	Bootmethod device to use
	 * @bflow:	On entry, provides dev, hwpart, part and method.
	 *	Returns updated bootflow if found
	 * @return 0 if OK, -ve on error
	 */
	int (*read_bootflow)(struct udevice *dev, struct bootflow *bflow);

	/**
	 * read_file() - read a file needed for a bootflow
	 *
	 * Read a file from the same place as the bootflow came from
	 *
	 * @dev:	Bootmethod device to use
	 * @bflow:	Bootflow providing info on where to read from
	 * @file_path:	Path to file (may be absolute or relative)
	 * @addr:	Address to load file
	 * @sizep:	On entry provides the maximum permitted size; on exit
	 *		returns the size of the file
	 * @return 0 if OK, -ENOSPC if the file is too large for @sizep, other
	 *	-ve value if something else goes wrong
	 */
	int (*read_file)(struct udevice *dev, struct bootflow *bflow,
			 const char *file_path, ulong addr, ulong *sizep);

	/**
	 * boot() - boot a bootflow
	 *
	 * @dev:	Bootmethod device to boot
	 * @bflow:	Bootflow to boot
	 * @return does not return on success, since it should boot the
	 *	Operating Systemn. Returns -EFAULT if that fails, -ENOTSUPP if
	 *	trying method resulted in finding out that is not actually
	 *	supported for this boot and should not be tried again unless
	 *	something changes, other -ve on other error
	 */
	int (*boot)(struct udevice *dev, struct bootflow *bflow);
};

#define bootmeth_get_ops(dev)  ((struct bootmeth_ops *)(dev)->driver->ops)

/**
 * bootmeth_check() - check if a bootmeth supports this bootflow
 *
 * This is optional. If not provided, the bootdev is assumed to be
 * supported
 *
 * The bootmeth can check the bootdev (e.g. to make sure it is a
 * network device) or the partition information. The following fields
 * in @iter are available:
 *
 *   name, dev, state, part
 *   max_part may be set if part != 0 (i.e. there is a valid partition
 *      table). Otherwise max_part is 0
 *   method is available but is the same as @dev
 *   the partition has not yet been read, nor has the filesystem been
 *   checked
 *
 * It may update only the flags in @iter
 *
 * @dev:	Bootmethod device to check against
 * @iter:	On entry, provides bootdev, hwpart, part
 * @return 0 if OK, -ENOTSUPP if this bootdev is not supported
 */
int bootmeth_check(struct udevice *dev, struct bootflow_iter *iter);

/**
 * bootmeth_read_bootflow() - set up a bootflow for a device
 *
 * @dev:	Bootmethod device to check
 * @bflow:	On entry, provides dev, hwpart, part and method.
 *	Returns updated bootflow if found
 * @return 0 if OK, -ve on error
 */
int bootmeth_read_bootflow(struct udevice *dev, struct bootflow *bflow);

/**
 * bootmeth_read_file() - read a file needed for a bootflow
 *
 * Read a file from the same place as the bootflow came from
 *
 * @dev:	Bootmethod device to use
 * @bflow:	Bootflow providing info on where to read from
 * @file_path:	Path to file (may be absolute or relative)
 * @addr:	Address to load file
 * @sizep:	On entry provides the maximum permitted size; on exit
 *		returns the size of the file
 * @return 0 if OK, -ENOSPC if the file is too large for @sizep, other
 *	-ve value if something else goes wrong
 */
int bootmeth_read_file(struct udevice *dev, struct bootflow *bflow,
		       const char *file_path, ulong addr, ulong *sizep);

/**
 * bootmeth_boot() - boot a bootflow
 *
 * @dev:	Bootmethod device to boot
 * @bflow:	Bootflow to boot
 * @return does not return on success, since it should boot the
 *	Operating Systemn. Returns -EFAULT if that fails, other -ve on
 *	other error
 */
int bootmeth_boot(struct udevice *dev, struct bootflow *bflow);

/**
 * bootmeth_setup_iter_order() - Set up the ordering of bootmeths to scan
 *
 * This sets up the ordering information in @iter, based on the selected
 * ordering of the bootmethds in bootstd_priv->bootmeth_order. If there is no
 * ordering there, then all bootmethods are added
 *
 * @iter: Iterator to update with the order
 * @return 0 if OK, -ENOENT if no bootdevs, -ENOMEM if out of memory, other -ve
 *	on other error
 */
int bootmeth_setup_iter_order(struct bootflow_iter *iter);

/**
 * bootmeth_set_order() - Set the bootmeth order
 *
 * This selects the ordering to use for bootmeths
 *
 * @order_str: String containing the ordering. This is a comma-separate list of
 * bootmeth-device names, e.g. "syslinux,efi". If empty then a default ordering
 * is used, based on the sequence number of devices (i.e. using aliases)
 * @return 0 if OK, -ENODEV if an unknown bootmeth is mentioned, -ENOMEM if
 * out of memory, -ENOENT if there are no bootmeth devices
 */
int bootmeth_set_order(const char *order_str);

#endif
