/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __RAM_H
#define __RAM_H

struct ram_info {
	phys_addr_t base;
	size_t size;
};

struct ram_ops {
	/**
	 * get_info() - Get basic memory info
	 *
	 * @dev:	Device to check (UCLASS_RAM)
	 * @info:	Place to put info
	 * @return 0 if OK, -ve on error
	 */
	int (*get_info)(struct udevice *dev, struct ram_info *info);
};

#define ram_get_ops(dev)        ((struct ram_ops *)(dev)->driver->ops)

/**
 * ram_get_info() - Get information about a RAM device
 *
 * @dev:	Device to check (UCLASS_RAM)
 * @info:	Returns RAM info
 * @return 0 if OK, -ve on error
 */
int ram_get_info(struct udevice *dev, struct ram_info *info);

/**
 * struct tiny_ram_ops - Operations for tiny RAM devices
 */
struct tiny_ram_ops {
	/**
	 * get_info() - Get basic memory info
	 *
	 * @dev:	Device to check (UCLASS_RAM)
	 * @info:	Place to put info
	 * @return 0 if OK, -ve on error
	 */
	int (*get_info)(struct tinydev *dev, struct ram_info *info);
};

#define tiny_ram_get_ops(dev)	((struct tiny_ram_ops *)(dev)->drv->ops)

/**
 * tiny_ram_get_info() - Get information about a RAM device
 *
 * @dev:	Device to check (UCLASS_RAM)
 * @info:	Returns RAM info
 * @return 0 if OK, -ve on error
 */
int tiny_ram_get_info(struct tinydev *tdev, struct ram_info *info);

#endif
