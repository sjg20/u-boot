/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootdev_h
#define __bootdev_h

#include <linux/list.h>

struct bootflow;
struct bootflow_iter;
struct bootdev_state;
struct udevice;

/**
 * struct bootdev_state - information about available bootflows, etc.
 *
 * This is attached to the bootdev uclass so there is only one of them. It
 * provides overall information about bootdevs and bootflows.
 *
 * @cur_bootdev: Currently selected bootdev (for commands)
 * @cur_bootflow: Currently selected bootflow (for commands)
 * @glob_head: Head for the global list of all bootdevs across all bootflows
 * @bootmeth_count: Number of bootmeth devices in @bootmeth_order
 * @bootmeth_order: List of bootmeth devices to use, in order, NULL-terminated
 */
struct bootdev_state {
	struct udevice *cur_bootdev;
	struct bootflow *cur_bootflow;
	struct list_head glob_head;
	int bootmeth_count;
	struct udevice **bootmeth_order;
};

/**
 * enum bootdev_prio_t - priority of each bootdev
 *
 * These values are associated with each bootdev and set up by the driver.
 *
 * Smallest value is the highest priority. By default, bootdevs are scanned from
 * highest to lowest priority
 */
enum bootdev_prio_t {
	BOOTDEVP_0_INTERNAL_FAST	= 10,
	BOOTDEVP_1_INTERNAL_SLOW	= 20,
	BOOTDEVP_2_SCAN_FAST		= 30,
	BOOTDEVP_3_SCAN_SLOW		= 40,
	BOOTDEVP_4_NET_BASE		= 50,
	BOOTDEVP_5_NET_FALLBACK		= 60,

	BOOTDEVP_COUNT,
};

/**
 * struct bootdev_uc_plat - uclass information about a bootdev
 *
 * This is attached to each device in the bootdev uclass and accessible via
 * dev_get_uclass_plat(dev)
 *
 * @bootflows: List of available bootflows for this bootdev
 * @piro: Priority of this bootdev
 */
struct bootdev_uc_plat {
	struct list_head bootflow_head;
	enum bootdev_prio_t prio;
};

/**
 * struct bootdev_ops - Operations for the bootdev uclass
 *
 * Consider using ioctl() to add rarely used or driver-specific operations.
 */
struct bootdev_ops {
	/**
	 * get_bootflow() - get a bootflow
	 *
	 * @dev:	Bootflow device to check
	 * @iter:	Provides current dev, part, method to get. Should update
	 *	max_part if there is a partition table
	 * @bflow:	Updated bootflow if found
	 * @return 0 if OK, -ESHUTDOWN if there are no more bootflows on this
	 *	device, -ENOSYS if this device doesn't support bootflows,
	 *	other -ve value on other error
	 */
	int (*get_bootflow)(struct udevice *dev, struct bootflow_iter *iter,
			    struct bootflow *bflow);
};

#define bootdev_get_ops(dev)  ((struct bootdev_ops *)(dev)->driver->ops)

/**
 * bootdev_get_bootflow() - get a bootflow
 *
 * @dev:	Bootflow device to check
 * @iter:	Provides current  part, method to get
 * @bflow:	Returns bootflow if found
 * @return 0 if OK, -ESHUTDOWN if there are no more bootflows on this device,
 *	-ENOSYS if this device doesn't support bootflows, other -ve value on
 *	other error
 */
int bootdev_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			 struct bootflow *bflow);

/**
 * bootdev_bind() - Bind a new named bootdev device
 *
 * @parent:	Parent of the new device
 * @drv_name:	Driver name to use for the bootdev device
 * @name:	Name for the device (parent name is prepended)
 * @devp:	the new device (which has not been probed)
 */
int bootdev_bind(struct udevice *parent, const char *drv_name, const char *name,
		 struct udevice **devp);

/**
 * bootdev_find_in_blk() - Find a bootdev in a block device
 *
 * @dev: Bootflow device associated with this block device
 * @blk: Block device to search
 * @iter:	Provides current dev, part, method to get. Should update
 *	max_part if there is a partition table
 * @bflow: On entry, provides information about the partition and device to
 *	check. On exit, returns bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootdev_find_in_blk(struct udevice *dev, struct udevice *blk,
			struct bootflow_iter *iter, struct bootflow *bflow);

/**
 * bootdev_list() - List all available bootdevs
 *
 * @probe: true to probe devices, false to leave them as is
 */
void bootdev_list(bool probe);

/**
 * bootdev_get_state() - Get the (single) state for the bootdev system
 *
 * The state holds a global list of all bootflows that have been found.
 *
 * @return 0 if OK, -ve if the uclass does not exist
 */
int bootdev_get_state(struct bootdev_state **statep);

/**
 * bootdev_clear_bootflows() - Clear bootflows from a bootdev
 *
 * Each bootdev maintains a list of discovered bootflows. This provides a
 * way to clear it. These bootflows are removed from the global list too.
 *
 * @dev: bootdev device to update
 */
void bootdev_clear_bootflows(struct udevice *dev);

/**
 * bootdev_clear_glob() - Clear the global list of bootflows
 *
 * This removes all bootflows globally and across all bootdevs.
 */
void bootdev_clear_glob(void);

/**
 * bootdev_add_bootflow() - Add a bootflow to the bootdev's list
 *
 * All fields in @bflow must be set up. Note that @bflow->dev is used to add the
 * bootflow to that device.
 *
 * @dev: Bootdevice device to add to
 * @bflow: Bootflow to add. Note that fields within bflow must be allocated
 *	since this function takes over ownership of these. This functions makes
 *	a copy of @bflow itself (without allocating its fields again), so the
 *	caller must dispose of the memory used by the @bflow pointer itself
 * @return 0 if OK, -ENOMEM if out of memory
 */
int bootdev_add_bootflow(struct bootflow *bflow);

/**
 * bootdev_first_bootflow() - Get the first bootflow from a bootdev
 *
 * Returns the first bootflow attached to a bootdev
 *
 * @dev: bootdev device
 * @bflowp: Returns a pointer to the bootflow
 * @return 0 if found, -ENOENT if there are no bootflows
 */
int bootdev_first_bootflow(struct udevice *dev, struct bootflow **bflowp);

/**
 * bootdev_next_bootflow() - Get the next bootflow from a bootdev
 *
 * Returns the next bootflow attached to a bootdev
 *
 * @bflowp: On entry, the last bootflow returned , e.g. from
 *	bootdev_first_bootflow()
 * @return 0 if found, -ENOENT if there are no more bootflows
 */
int bootdev_next_bootflow(struct bootflow **bflowp);

/**
 * bootdev_find_by_label() - Look up a bootdev by label
 *
 * Each bootdev has a label which contains the media-uclass name and a number,
 * e.g. 'mmc2'. This looks up the label and returns the associated bootdev
 *
 * The lookup is performed based on the media device's sequence number. So for
 * 'mmc2' this looks for a device in UCLASS_MMC with a dev_seq() of 2.
 *
 * @label: Label to look up (e.g. "mmc1" or "mmc0")
 * @devp: Returns the bootdev device found, or NULL if none (note it does not
 *	return the media device, but its bootdev child)
 * @return 0 if OK, -EINVAL if the uclass is not supported by this board,
 *	-ENOENT if there is no device with that number
 */
int bootdev_find_by_label(const char *label, struct udevice **devp);

/**
 * bootdev_find_by_any() - Find a bootdev by name, label or sequence
 *
 * @name: name (e.g. "mmc2.bootdev"), label ("mmc2"), or sequence ("2") to find
 * @devp: returns the device found, on success
 * @return 0 if OK, -ve on error
 */
int bootdev_find_by_any(const char *name, struct udevice **devp);

#if CONFIG_IS_ENABLED(BOOTDEV)
/**
 * bootdev_setup_for_dev() - Bind a new bootdev device
 *
 * Creates a bootdev device as a child of @parent. This should be called from
 * the driver's bind() method or its uclass' post_bind() method.
 *
 * @parent: Parent device (e.g. MMC or Ethernet)
 * @drv_name: Name of bootdev driver to bind
 * @return 0 if OK, -ve on error
 */
int bootdev_setup_for_dev(struct udevice *parent, const char *drv_name);

/**
 * bootdev_unbind_dev() - Unbind a bootdev device
 *
 * Remove and unbind a bootdev device which is a child of @parent. This should
 * be called from the driver's unbind() method or its uclass' post_bind()
 * method.
 *
 * @parent: Parent device (e.g. MMC or Ethernet)
 * @return 0 if OK, -ve on error
 */
int bootdev_unbind_dev(struct udevice *parent);
#else
static inline int bootdev_setup_for_dev(struct udevice *parent,
					const char *drv_name)
{
	return 0;
}
#endif

#endif
