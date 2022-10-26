/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * sandbox host uclass
 *
 * Copyright 2022 Google LLC
 */

#ifndef __SANDBOX_HOST__
#define __SANDBOX_HOST__

/**
 * struct host_sb_plat - platform data for a host device
 *
 * @label: Label for this device (allocated)
 * @filename: Name of file this is attached to, or NULL (allocated)
 * @fd: File descriptor of file, or 0 for none (file is not open)
 */
struct host_sb_plat {
	char *label;
	char *filename;
	int fd;
};

/**
 * struct host_ops - operations supported by UCLASS_HOST
 *
 * @attach_file: Attach a new file to a device
 * @detach_file: Detach a file from a device
 */
struct host_ops {
	/*
	 * attach_file() - Attach a new file to the device
	 *
	 * @dev: Device to update
	 * @filename: Name of the file, e.g. "/path/to/disk.img"
	 * @removable: true if the device should be marked as removable, false
	 *	if it is fixed. See enum blk_flag_t
	 * Returns: 0 if OK, -EEXIST if a file is already attached, other -ve on
	 * other error
	 */
	int (*attach_file)(struct udevice *dev, const char *filename,
			   bool removable);

	/**
	 * detach_file() - Detach a file from the device
	 *
	 * @dev: Device to detach from
	 * Returns: 0 if OK, -ENOENT if no file is attached, other -ve on other
	 * error
	 */
	 int (*detach_file)(struct udevice *dev);
};

#define host_get_ops(dev)        ((struct host_ops *)(dev)->driver->ops)

/**
 * host_attach_file() - Attach a new file to the device
 *
 * @dev: Device to update
 * @filename: Name of the file, e.g. "/path/to/disk.img"
 * @removable: true if the device should be marked as removable, false
 *	if it is fixed. See enum blk_flag_t
 * Returns: 0 if OK, -EEXIST if a file is already attached, other -ve on
 * other error
 */
int host_attach_file(struct udevice *dev, const char *filename, bool removable);

/**
 * host_detach_file() - Detach a file from the device
 *
 * @dev: Device to detach from
 * Returns: 0 if OK, -ENOENT if no file is attached, other -ve on other
 * error
 */
int host_detach_file(struct udevice *dev);

/**
 * host_create_device() - Create a new host device
 *
 * @label: Label of the attachment, e.g. "test1"
 * @devp: Returns the device created, on success
 * Returns: 0 if OK, -ve on error
 */
int host_create_device(const char *label, struct udevice **devp);

/**
 * host_create_attach_file() - Create a new host device attached to a file
 *
 * @label: Label of the attachment, e.g. "test1"
 * @filename: Name of the file, e.g. "/path/to/disk.img"
 * @removable: true if the device should be marked as removable, false
 *	if it is fixed. See enum blk_flag_t
 * @devp: Returns the device created, on success
 * Returns: 0 if OK, -ve on error
 */
int host_create_attach_file(const char *label, const char *filename,
			    bool removable, struct udevice **devp);

/**
 * host_find_by_label() - Find a host by label
 *
 * Searches all host devices to find one with the given label
 *
 * @label: Label to find
 * Returns: associated device, or NULL if not found
 */
struct udevice *host_find_by_label(const char *label);

/**
 * host_get_cur_dev() - Get the current device
 *
 * Returns current device, or NULL if none
 */
struct udevice *host_get_cur_dev(void);

/**
 * host_set_cur_dev() - Set the current device
 *
 * Sets the current device, or clears it if @dev is NULL
 *
 * @dev: Device to set as the current one
 */
void host_set_cur_dev(struct udevice *dev);

#endif /* __SANDBOX_HOST__ */
