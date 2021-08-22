/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Chromium OS vboot EC uclass, used for vboot operations implemented by an EC
 * that uses the Chromium OS code base / messages.
 *
 * Copyright 2018 Google LLC
 */

#ifndef __CROS_VBOOT_EC_H
#define __CROS_VBOOT_EC_H

struct abuf;

enum {
	/* Maximum size of the hash value for an EC image */
	VBOOT_EC_MAX_HASH_SIZE	= 64,
};

/**
 * struct vboot_ec_uc_priv - Uclass information about each vboot EC
 *
 * @hash_digest: Value of the hash digest for this vboot EC
 */
struct vboot_ec_uc_priv {
	u8 hash_digest[VBOOT_EC_MAX_HASH_SIZE];
};

/**
 * struct vboot_ec_ops - EC operations required by vboot
 *
 * These directly correspond to the vboot VbExEc... interfaces.
 */
struct vboot_ec_ops {
	/**
	 * running_rw() - Check if the EC is currently running rewriteable code
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @in_rwp: Returns 0 if the EC is in RO code, 1 if not
	 * @return 0 if OK, other value if the current EC image is unknown
	 */
	int (*running_rw)(struct udevice *dev, int *in_rwp);

	/** jump_to_rw() - Request the EC jump to its rewriteable code
	 *
	 * If successful, returns when the EC has booting its RW code far enough
	 * to respond to subsequent commands. Does nothing if the EC is already
	 * in its rewriteable code.
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @return 0 if OK, non-zero on error
	 */
	int (*jump_to_rw)(struct udevice *dev);

	/**
	 * disable_jump() - Tell the EC to refuse another jump until it reboots
	 *
	 * After this is called, subsequent calls to jump_to_rw() in this boot
	 * will fail
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @return 0 if OK, non-zero on error
	 */
	int (*disable_jump)(struct udevice *dev);

	/**
	 * hash_image() - Read the SHA-256 hash of the selected EC image
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @select:	Image to get hash of. RO or RW
	 * @hash:	Pointer to the hash
	 * @hash_sizep:	Pointer to the hash size, which is set to the
	 *	maximum allowed size on entry and must be updated to the actual
	 *	size on exit
	 *
	 * @return 0 if OK, non-zero on error
	 */
	int (*hash_image)(struct udevice *dev, enum vb2_firmware_selection select,
			  u8 *hash, int *hash_sizep);

	/**
	 * update_image() - Update the selected EC image
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @select:	Image to get hash of. RO or RW
	 * @image:	Pointer to the image
	 * @image_size:	Size of the image in bytes
	 * @return 0 if OK, non-zero on error
	 */
	int (*update_image)(struct udevice *dev, enum vb2_firmware_selection select,
			    const struct abuf *buf);

	/**
	 * protect() - Lock the selected EC code until the EC is rebooted
	 *
	 * This prevents updates until the EC is rebooted. Subsequent calls to
	 * update_image() with the same region this boot will fail.
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @select:	Image to protect
	 * @return 0 if OK, -EPERM if protection could not be set and a reboot
	 *	is required, other non-zero on error
	 */
	int (*protect)(struct udevice *dev, enum vb2_firmware_selection select);

	/**
	 * reboot_to_ro() Tells the EC to reboot to RO on next AP shutdown
	 *
	 * @dev: UCLASS_CROS_VBOOT_EC device
	 * @return 0 if OK, non-zero on error
	 */
	int (*reboot_to_ro)(struct udevice *dev);
};

#define vboot_ec_get_ops(dev)	((struct vboot_ec_ops *)(dev)->driver->ops)

/* See above for comments for these wrapper functions */
int vboot_ec_running_rw(struct udevice *dev, int *in_rwp);
int vboot_ec_jump_to_rw(struct udevice *dev);
int vboot_ec_disable_jump(struct udevice *dev);
int vboot_ec_hash_image(struct udevice *dev, enum vb2_firmware_selection select,
			const u8 **hashp, int *hash_sizep);
int vboot_ec_update_image(struct udevice *dev, enum vb2_firmware_selection select,
			  const struct abuf *buf);
int vboot_ec_protect(struct udevice *dev, enum vb2_firmware_selection select);
int vboot_ec_reboot_to_ro(struct udevice *dev);

#endif /* __CROS_VBOOT_EC_H */
