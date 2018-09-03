/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Non-volatile data access (TPM, CMOS RAM, Chromium OS EC, etc.). This provides
 * access to a small amount of data (e.g. 16 bytes) that survives a normal
 * reboot.
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __CROS_NVDATA_H
#define __CROS_NVDATA_H

/* These match with <dt-bindings/cros/nvdata.h> */

/**
 * enum cros_nvdata_type - types of non-volatile data
 *
 * @CROS_NV_DATA: Standard data (can be lost)
 * @CROS_NV_SECDATA: Secure data (e.g. stored in TPM)
 * @CROS_NV_SECDATAK: Secure data for kernel
 * @CROS_NV_REC_HASH: Recovery-mode hash
 * @CROS_NV_VSTORE: Verified boot storage slot 0
 */
enum cros_nvdata_type {
	CROS_NV_DATA	= 0,
	CROS_NV_SECDATA,
	CROS_NV_SECDATAK,
	CROS_NV_REC_HASH,
	CROS_NV_VSTORE,
};

/* TPM NVRAM location indices */
#define FIRMWARE_NV_INDEX               0x1007
#define KERNEL_NV_INDEX                 0x1008
/*
 * 0x1009 used to be used as a backup space. Think of conflicts if you
 * want to use 0x1009 for something else.
 */
#define BACKUP_NV_INDEX                 0x1009
#define FWMP_NV_INDEX                   0x100a
#define REC_HASH_NV_INDEX               0x100b
#define REC_HASH_NV_SIZE                VB2_SHA256_DIGEST_SIZE

/**
 * struct nvdata_uc_priv - private uclass data for each device
 *
 * @supported: Bit mask of which enum cros_nvdata_type types are supported by
 *	this device (bit 0 = CROS_NV_DATA)
 */
struct nvdata_uc_priv {
	u32 supported;
};

/* Operations for the Platform Controller Hub */
struct cros_nvdata_ops {
	/**
	 * read() - read non-volatile data
	 *
	 * Read data that was previously written to the device
	 *
	 * @dev:	Device to read from
	 * @type:	Type of data to read
	 * @data:	Buffer for data read
	 * @len:	Length of data to read
	 * @return 0 if OK, -ENOSYS if the driver does not support this index,
	 *	-ENOENT if the data for this index is supported but has not been
	 *	written yet, -EMSGSIZE if the length does not match
	 *	expectations, -EIO if the device failed, other -ve value on
	 *	other error
	 */
	int (*read)(struct udevice *dev, enum cros_nvdata_type type, u8 *data,
		    int size);

	/**
	 * write() - write non-volatile data
	 *
	 * This writes data in a non-volatile manner so that it can be read
	 * back later
	 *
	 * @dev:	Device to write to
	 * @type:	Type of data to write
	 * @data:	Buffer for data write
	 * @len:	Length of data to write
	 * @return 0 if OK, -EMSGSIZE if the length does not match expectations,
	 *	-EIO if the device failed, other -ve value on other error
	 */
	int (*write)(struct udevice *dev, enum cros_nvdata_type type,
		     const u8 *data, int size);

	/**
	 * setup() - set up the data in the device
	 *
	 * This sets things up so that we can write data to a particular area
	 * of the non-volatile memory.
	 *
	 * @dev:	Device to update
	 * @type:	Type of data to setup
	 * @attr:	Device-specific attributes for the index
	 * @size:	Size of data space to set up
	 * @nv_policy:	Device-specific policy data (NULL for none)
	 * @nv_policy_size: Size of device-specific policy data
	 * @return 0 if OK, -ve value on error
	 */
	int (*setup)(struct udevice *dev, enum cros_nvdata_type type,
		     uint attr, uint size,
		     const u8 *nv_policy, int nv_policy_size);

	/**
	 * lock() - lock the data so it cannot be written until reboot
	 *
	 * Once this operation is completed successfully, it should not be
	 * possible to write to the data again until the device is rebooted
	 *
	 * @dev:	Device to update
	 * @type:	Type of data to lock
	 * @return 0 if OK, -ve on error
	 */
	int (*lock)(struct udevice *dev, enum cros_nvdata_type);
};

#define cros_nvdata_get_ops(dev) ((struct cros_nvdata_ops *)(dev)->driver->ops)

/**
 * cros_nvdata_read() - read non-volatile data
 *
 * Read data that was previously written to the device
 *
 * @dev:	Device to read from
 * @type:	Type of data to read
 * @data:	Buffer for data read
 * @len:	Length of data to read
 * @return 0 if OK, -ENOSYS if the driver does not support this index,
 *	-ENOENT if the data for this index is supported but has not been
 *	written yet, -EMSGSIZE if the length does not match
 *	expectations, -EIO if the device failed, other -ve value on
 *	other error
 */
int cros_nvdata_read(struct udevice *dev, enum cros_nvdata_type type,
		     u8 *data, int size);

/**
 * cros_nvdata_write() - write non-volatile data
 *
 * This writes data in a non-volatile manner so that it can be read
 * back later
 *
 * @dev:	Device to write to
 * @type:	Type of data to write
 * @data:	Buffer for data write
 * @len:	Length of data to write
 * @return 0 if OK, -EMSGSIZE if the length does not match expectations,
 *	-EIO if the device failed, other -ve value on other error
 */
int cros_nvdata_write(struct udevice *dev, enum cros_nvdata_type type,
		      const u8 *data, int size);

/**
 * cros_nvdata_setup() - set up the data in the device
 *
 * This sets things up so that we can write data to a particular area
 * of the non-volatile memory.
 *
 * @dev:	Device to update
 * @type:	Type of data to setup
 * @attr:	Device-specific attributes for the index
 * @size:	Size of data space to set up
 * @nv_policy:	Device-specific policy data (NULL for none)
 * @nv_policy_size: Size of device-specific policy data
 * @return 0 if OK, -ve value on error
 */
int cros_nvdata_setup(struct udevice *dev, enum cros_nvdata_type type,
		      uint attr, uint size,
		      const u8 *nv_policy, int nv_policy_size);

/**
 * cros_nvdata_lock() - lock the data so it cannot be written until reboot
 *
 * Once this operation is completed successfully, it should not be
 * possible to write to the data again until the device is rebooted
 *
 * @dev:	Device to update
 * @type:	Type of data to lock
 * @return 0 if OK, -ve on error
 */
int cros_nvdata_lock(struct udevice *dev, enum cros_nvdata_type type);

/**
 * cros_nvdata_read_walk() - walk all devices to read non-volatile data
 *
 * Read data that was previously written to a device
 *
 * @type:	Type of data to read
 * @data:	Buffer for data read
 * @len:	Length of data to read
 * @return 0 if OK, -ENOENT if the data for this index is supported but has not
 *	been written yet, -EMSGSIZE if the length does not match expectations,
 *	 -EIO if the device failed, -ENOSYS if no device could process this
 *	request, other -ve value on other error
 */
int cros_nvdata_read_walk(enum cros_nvdata_type type, u8 *data, int size);

/**
 * cros_nvdata_write_walk() - walk all devices to write non-volatile data
 *
 * This writes data in a non-volatile manner so that it can be read
 * back later
 *
 * @type:	Type of data to write
 * @data:	Buffer for data write
 * @len:	Length of data to write
 * @return 0 if OK, -EMSGSIZE if the length does not match expectations,
 *	-EIO if the device failed, -ENOSYS if no device could process this
 *	request, other -ve value on other error
 */
int cros_nvdata_write_walk(enum cros_nvdata_type type, const u8 *data,
			   int size);

/**
 * cros_nvdata_setup_walk() - walk all devices set up the data in the device
 *
 * This sets things up so that we can write data to a particular area
 * of the non-volatile memory.
 *
 * @type:	Type of data to setup
 * @attr:	Device-specific attributes for the index
 * @size:	Size of data space to set up
 * @nv_policy:	Device-specific policy data (NULL for none)
 * @nv_policy_size: Size of device-specific policy data
 * @return 0 if OK, -ENOSYS if no device could process this request, other -ve
 *	 value on error
 */
int cros_nvdata_setup_walk(enum cros_nvdata_type type, uint attr, uint size,
			   const u8 *nv_policy, uint nv_policy_size);

/**
 * cros_nvdata_lock_walk() - walk all devices to lock data
 *
 * Once this operation is completed successfully, it should not be
 * possible to write to the data again until the device is rebooted
 *
 * @type:	Type of data to lock
 * @return 0 if OK, -ENOSYS if no device could process this request, other -ve
 *	value  on error
 */
int cros_nvdata_lock_walk(enum cros_nvdata_type );

/**
 * cros_nvdata_of_to_plat() - Read data type from devicetree
 *
 * This reads information required by the uclass from the devicetree
 *
 * @dev: Device whose node is to be read
 */
int cros_nvdata_of_to_plat(struct udevice *dev);

#endif /* __CROS_NVDATA_H */
