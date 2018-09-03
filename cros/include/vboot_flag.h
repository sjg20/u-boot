/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Implementation of verified-boot flags for Chromium OS. These are hardware or
 * secure switches which control verified boot.
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __CROS_VBOOT_FLAG_H
#define __CROS_VBOOT_FLAG_H

/* Available vboot flags for Chromium OS */
enum vboot_flag_t {
	VBOOT_FLAG_WRITE_PROTECT = 0,
	VBOOT_FLAG_DEVELOPER,
	VBOOT_FLAG_LID_OPEN,
	VBOOT_FLAG_POWER_OFF,
	VBOOT_FLAG_EC_IN_RW,
	VBOOT_FLAG_OPROM_LOADED,
	VBOOT_FLAG_RECOVERY,
	VBOOT_FLAG_WIPEOUT,

	VBOOT_FLAG_COUNT,
};

/**
 * struct vboot_flag_uc_priv - uclass information for each device
 *
 * Each device in this uclass has its own version of this struct.
 *
 * @flag: Flag that this device handles, so the uclass knows which devices to
 *	try
 */
struct vboot_flag_uc_priv {
	enum vboot_flag_t flag;
};

/** struct vboot_flag_ops - Operations for the verified boot flags */
struct vboot_flag_ops {
	/**
	 * read() - read flag value
	 *
	 * @dev:	Device to read from
	 * @return flag value if OK (0 or 1), -ENOENT if no driver supports
	 *	the flag, -ve on error
	 */
	int (*read)(struct udevice *dev);
};

#define vboot_flag_get_ops(dev) ((struct vboot_flag_ops *)(dev)->driver->ops)

/**
 * vboot_flag_read() - read vboot flag
 *
 * @dev:	Device to read from
 * @return flag value if OK, -ve opn error
 */
int vboot_flag_read(struct udevice *dev);

/**
 * vboot_flag_read_walk() - Walk through all devices to find a flag value
 *
 * This finds the appropriate device for a particular flag and returns its value
 *
 * @flag: Flag to find
 * @return flag value (0 or 1) if OK, -ENOENT if no driver supports the flag,
 *	-E2BIG if more than one driver supports the flag, other -ve
 *	value on other error
 */
int vboot_flag_read_walk(enum vboot_flag_t flag);

/**
 * vboot_flag_read_walk_prev() - Walk through all devices to find a flag value
 *
 * This finds the appropriate device for a particular flag and returns its
 * previous value, as stored last time the value was read
 *
 * @flag: Flag to find
 * @prevp: Returns previous value of flag on success (-1 if there is no previous
 *	value)
 * @devp: Returns device that provided the flag value
 * @return flag value (0 or 1) if OK, -ENOENT if no driver supports the flag,
 *	-E2BIG if more than one driver supports the flag, other -ve
 *	value on other error
 */
int vboot_flag_read_walk_prev(enum vboot_flag_t flag, int *prevp,
			      struct udevice **devp);

/**
 * vboot_flag_name() - Get the name of a flag
 *
 * @flag: Flag to check
 * @return name of flag
 */
const char *vboot_flag_name(enum vboot_flag_t flag);

#endif /* __CROS_VBOOT_FLAG_H */
