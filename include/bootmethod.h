/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootmethod_h
#define __bootmethod_h

#include <linux/list.h>

/**
 * enum bootflow_state_t - states that a particular bootflow can be in
 */
enum bootflow_state_t {
	BOOTFLOWST_BASE,	/**< Nothing known yet */
	BOOTFLOWST_MEDIA,	/**< Media exists */
	BOOTFLOWST_PART,	/**< Partition exists */
	BOOTFLOWST_FS,		/**< Filesystem exists */
	BOOTFLOWST_FILE,	/**< Bootflow file exists */
	BOOTFLOWST_LOADED,	/**< Bootflow file loaded */

	BOOTFLOWST_COUNT
};

enum bootflow_type_t {
	BOOTFLOWT_DISTRO,	/**< Distro boot */

	BOOTFLOWT_COUNT,
};

/**
 * struct bootflow_state - information about available bootflows, etc.
 *
 * This is attached to the bootmethod uclass so there is only one of them. It
 * provides overall information about bootmethods and bootflows.
 *
 * @cur_bootmethod: Currently selected bootmethod (for commands)
 * @cur_bootflow: Currently selected bootflow (for commands)
 * @glob_head: Head for the global list of all bootmethods across all bootflows
 */
struct bootflow_state {
	struct udevice *cur_bootmethod;
	struct bootflow *cur_bootflow;
	struct list_head glob_head;
};

/**
 * struct bootmethod_uc_priv - uclass information about a bootmethod
 *
 * This is attached to each device in the bootmethod uclass and accessible via
 * dev_get_uclass_priv(dev)
 *
 * @bootflows: List of available bootflows for this bootmethod
 */
struct bootmethod_uc_priv {
	struct list_head bootflow_head;
};

extern struct bootflow_cmds g_bootflow_cmds;

/**
 * struct bootflow - information about a bootflow
 *
 * This is connected into two separate linked lists:
 *
 *   bm_sibling - links all bootflows in the same bootmethod
 *   glob_sibling - links all bootflows in all bootmethods
 *
 * @bm_node: Points to siblings in the same bootmethod
 * @glob_node: Points to siblings in the global list (all bootmethod)
 * @dev: Bootmethod device which produced this bootflow
 * @blk: Block device which contains this bootflow
 * @seq: Sequence number of bootflow within its bootmethod, typically the
 *	partition number (0...)
 * @name: Name of bootflow (allocated)
 * @type: Bootflow type (enum bootflow_type_t)
 * @state: Current state (enum bootflow_state_t)
 * @part: Partition number
 * @fname: Filename of bootflow file (allocated)
 * @buf: Bootflow file contents (allocated)
 * @size: Size of bootflow in bytes
 * @err: Error number received (0 if OK)
 */
struct bootflow {
	struct list_head bm_node;
	struct list_head glob_node;
	struct udevice *dev;
	struct udevice *blk;
	int seq;
	char *name;
	enum bootflow_type_t type;
	enum bootflow_state_t state;
	int part;
	char *fname;
	char *buf;
	int size;
	int err;
};

/**
 * enum bootflow_flags_t - flags for the bootflow
 *
 * @BOOTFLOWF_FIXED: Only used fixed/internal media
 * @BOOTFLOWF_SHOW: Show each bootmethod before scanning it
 * @BOOTFLOWF_ALL: Return bootflows with errors as well
 */
enum bootflow_flags_t {
	BOOTFLOWF_FIXED		= 1 << 0,
	BOOTFLOWF_SHOW		= 1 << 1,
	BOOTFLOWF_ALL		= 1 << 2,
};

/**
 * struct bootmethod_iter - state for iterating through bootflows
 *
 * @flags: Flags to use (see enum bootflow_flags_t)
 * @dev: Current bootmethod
 * @seq: Current sequence number within that bootmethod (determines partition
 *	number, for example)
 */
struct bootmethod_iter {
	int flags;
	struct udevice *dev;
	int seq;
};

/**
 * struct bootmethod_ops - Operations for the Platform Controller Hub
 *
 * Consider using ioctl() to add rarely used or driver-specific operations.
 */
struct bootmethod_ops {
	/**
	 * get_bootflow() - get a bootflow
	 *
	 * @dev:	Bootflow device to check
	 * @seq:	Sequence number of bootflow to read (0 for first)
	 * @bflow:	Returns bootflow if found
	 * @return sequence number of bootflow (>=0) if found, -ve on error
	 */
	int (*get_bootflow)(struct udevice *dev, int seq,
			    struct bootflow *bflow);
};

#define bootmethod_get_ops(dev)  ((struct bootmethod_ops *)(dev)->driver->ops)

/**
 * bootmethod_get_bootflow() - get a bootflow
 *
 * @dev:	Bootflow device to check
 * @seq:	Sequence number of bootflow to read (0 for first)
 * @bflow:	Returns bootflow if found
 * @return 0 if OK, -ve on error (e.g. there is no SPI base)
 */
int bootmethod_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow);

/**
 * bootmethod_scan_first_bootflow() - find the first bootflow
 *
 * This works through the available bootmethod devices until it finds one that
 * can supply a bootflow. It then returns that
 *
 * If @flags includes BOOTFLOWF_ALL then bootflows with errors are returned too
 *
 * @iter:	Place to store private info (inited by this call)
 * @flags:	Flags for bootmethod (enum bootflow_flags_t)
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootmethod_scan_first_bootflow(struct bootmethod_iter *iter, int flags,
				   struct bootflow *bflow);

/**
 * bootmethod_scan_next_bootflow() - find the next bootflow
 *
 * This works through the available bootmethod devices until it finds one that
 * can supply a bootflow. It then returns that bootflow
 *
 * @iter:	Private info (as set up by bootmethod_scan_first_bootflow())
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, -ve on error
 */
int bootmethod_scan_next_bootflow(struct bootmethod_iter *iter,
				  struct bootflow *bflow);

/**
 * bootmethod_bind() - Bind a new named bootmethod device
 *
 * @parent:	Parent of the new device
 * @drv_name:	Driver name to use for the bootmethod device
 * @name:	Name for the device (parent name is prepended)
 * @devp:	the new device (which has not been probed)
 */
int bootmethod_bind(struct udevice *parent, const char *drv_name,
		    const char *name, struct udevice **devp);

/**
 * bootmethod_find_in_blk() - Find a bootmethod in a block device
 *
 * @dev: Bootflow device containing this block device
 * @blk: Block device to search
 * @seq: Sequence number within block device, used as the partition number,
 *	after adding 1
 * @bflow:	Returns bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootmethod_find_in_blk(struct udevice *dev, struct udevice *blk, int seq,
			   struct bootflow *bflow);

/**
 * bootmethod_list() - List all available bootmethods
 *
 * @probe: true to probe devices, false to leave them as is
 */
void bootmethod_list(bool probe);

/**
 * bootmethod_state_get_name() - Get the name of a bootflow state
 *
 * @state: State to check
 * @return name, or "?" if invalid
 */
const char *bootmethod_state_get_name(enum bootflow_state_t state);

/**
 * bootmethod_type_get_name() - Get the name of a bootflow state
 *
 * @type: Type to check
 * @return name, or "?" if invalid
 */
const char *bootmethod_type_get_name(enum bootflow_type_t type);

int bootmethod_get_state(struct bootflow_state **statep);

void bootmethod_clear_bootflows(struct udevice *dev);

void bootmethod_clear_glob(void);

/**
 * bootmethod_add_bootflow() - Add a bootflow to the bootmethod's list
 *
 * All fields in @bflow must be set up. Note that @bflow->dev is used to add the
 * bootflow to that device.
 *
 * @dev: Bootmethod device to add to
 * @bflow: Bootflow to add. Note that fields within bflow must be allocated
 *	since this function takes over ownership of these. This functions makes
 *	a copy of @bflow itself (without allocating its fields again), so the
 *	caller must dispose of the memory used by the @bflow pointer itself
 * @return 0 if OK, -ENOMEM if out of memory
 */
int bootmethod_add_bootflow(struct bootflow *bflow);

int bootmethod_first_bootflow(struct udevice *dev, struct bootflow **bflowp);

int bootmethod_next_bootflow(struct bootflow **bflowp);

int bootflow_first_glob(struct bootflow **bflowp);

int bootflow_next_glob(struct bootflow **bflowp);

void bootflow_free(struct bootflow *bflow);

/**
 * bootflow_boot() - boot a bootflow
 *
 * @bflow: Bootflow to boot
 * @return -EPROTO if bootflow has not been loaded, -ENOSYS if the bootflow
 *	type is not supported, -EFAULT if the boot returned without an error
 *	when we are expecting it to boot
 */
int bootflow_boot(struct bootflow *bflow);

#endif
