/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootflow_h
#define __bootflow_h

#include <linux/list.h>

/**
 * enum bootflow_state_t - states that a particular bootflow can be in
 *
 * Only bootflows in state BOOTFLOWST_LOADED can be used to boot.
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

/**
 * struct bootflow - information about a bootflow
 *
 * This is connected into two separate linked lists:
 *
 *   bm_sibling - links all bootflows in the same bootdev
 *   glob_sibling - links all bootflows in all bootdevs
 *
 * @bm_node: Points to siblings in the same bootdev
 * @glob_node: Points to siblings in the global list (all bootdev)
 * @dev: Bootdevice device which produced this bootflow
 * @blk: Block device which contains this bootflow, NULL if this is a network
 *	device
 * @seq: Sequence number of bootflow within its bootdev
 * @part: Partition number (0 for whole device)
 * @method: Bootmethod device used to perform the boot and read files
 * @name: Name of bootflow (allocated)
 * @state: Current state (enum bootflow_state_t)
 * @subdir: Subdirectory to fetch files from (with trailing /), or NULL if none
 * @fname: Filename of bootflow file (allocated)
 * @buf: Bootflow file contents (allocated)
 * @size: Size of bootflow file in bytes
 * @err: Error number received (0 if OK)
 */
struct bootflow {
	struct list_head bm_node;
	struct list_head glob_node;
	struct udevice *dev;
	struct udevice *blk;
	int seq;
	int part;
	struct udevice *method;
	char *name;
	enum bootflow_state_t state;
	char *subdir;
	char *fname;
	char *buf;
	int size;
	int err;
};

/**
 * enum bootflow_flags_t - flags for the bootflow
 *
 * @BOOTFLOWF_FIXED: Only used fixed/internal media
 * @BOOTFLOWF_SHOW: Show each bootdev before scanning it
 * @BOOTFLOWF_ALL: Return bootflows with errors as well
 * @BOOTFLOWF_SINGLE_DEV: Just scan one bootmeth
 */
enum bootflow_flags_t {
	BOOTFLOWF_FIXED		= 1 << 0,
	BOOTFLOWF_SHOW		= 1 << 1,
	BOOTFLOWF_ALL		= 1 << 2,
	BOOTFLOWF_SINGLE_DEV	= 1 << 3,
};

/**
 * struct bootflow_iter - state for iterating through bootflows
 *
 * This starts at with the first bootdev/partition/bootmeth and can be used to
 * iterate through all of them.
 *
 * Iteration starts with the bootdev. The first partition (0, i.e. whole device)
 * is scanned first. For partition 0, it iterates through all the available
 * bootmeths to see which one(s) can provide a bootflow. Then it moves to
 * parition 1 (if there is one) and the process continues. Once all partitions
 * are examined, it moves to the next bootdev.
 *
 * Initially @max_part is 0, meaning that only the whole device (@part=0) can be
 * used. During scanning, if a partition table is found, then @max_part is
 * updated to a larger value, no less than the number of available partitions.
 * This ensures that iteration works through all partitions on the bootdev.
 *
 * @flags: Flags to use (see enum bootflow_flags_t)
 * @dev: Current bootdev
 * @part: Current partition number (0 for whole device)
 * @method: Current bootmeth
 * @max_part: Maximum hardware partition number in @dev, 0 if there is no
 *	partition table
 * @err: Error obtained from checking the last iteration. This is used to skip
 *	forward (e.g. to skip the current partition because it is not valid)
 *	-ENOTTY: try next partition
 *	-ESHUTDOWN: try next bootdev
 */
struct bootflow_iter {
	int flags;
	struct udevice *dev;
	int part;
	struct udevice *method;
	int max_part;
	int err;
};

/**
 * bootflow_reset_iter() - Reset a bootflow iterator
 *
 * This sets everything to the starting point, ready for use.
 *
 * @iter: Place to store private info (inited by this call)
 * @flags: Flags to use (see enum bootflow_flags_t)
 */
void bootflow_reset_iter(struct bootflow_iter *iter, int flags);

/**
 * bootflow_scan_bootdev() - find the first bootflow in a bootdev
 *
 * If @flags includes BOOTFLOWF_ALL then bootflows with errors are returned too
 *
 * @dev:	Boot device to scan, NULL to work through all of them until it
 *	finds one that * can supply a bootflow
 * @iter:	Place to store private info (inited by this call)
 * @flags:	Flags for bootdev (enum bootflow_flags_t)
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, other -ve on error
 */
int bootflow_scan_bootdev(struct udevice *dev, struct bootflow_iter *iter,
			  int flags, struct bootflow *bflow);

/**
 * bootflow_scan_first() - find the first bootflow
 *
 * This works through the available bootdev devices until it finds one that
 * can supply a bootflow. It then returns that
 *
 * If @flags includes BOOTFLOWF_ALL then bootflows with errors are returned too
 *
 * @iter:	Place to store private info (inited by this call)
 * @flags:	Flags for bootdev (enum bootflow_flags_t)
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, other -ve on error
 */
int bootflow_scan_first(struct bootflow_iter *iter, int flags,
			struct bootflow *bflow);

/**
 * bootflow_scan_next() - find the next bootflow
 *
 * This works through the available bootdev devices until it finds one that
 * can supply a bootflow. It then returns that bootflow
 *
 * @iter:	Private info (as set up by bootflow_scan_first())
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, -ve on error
 */
int bootflow_scan_next(struct bootflow_iter *iter, struct bootflow *bflow);

/**
 * bootflow_first_glob() - Get the first bootflow from the global list
 *
 * Returns the first bootflow in the global list, no matter what bootflow it is
 * attached to
 *
 * @bflowp: Returns a pointer to the bootflow
 * @return 0 if found, -ENOENT if there are no bootflows
 */
int bootflow_first_glob(struct bootflow **bflowp);

/**
 * bootflow_next_glob() - Get the next bootflow from the global list
 *
 * Returns the next bootflow in the global list, no matter what bootflow it is
 * attached to
 *
 * @bflowp: On entry, the last bootflow returned , e.g. from
 *	bootflow_first_glob()
 * @return 0 if found, -ENOENT if there are no more bootflows
 */
int bootflow_next_glob(struct bootflow **bflowp);

/**
 * bootflow_free() - Free memory used by a bootflow
 *
 * This frees fields within @bflow, but not the @bflow pointer itself
 */
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

/**
 * bootflow_state_get_name() - Get the name of a bootflow state
 *
 * @state: State to check
 * @return name, or "?" if invalid
 */
const char *bootflow_state_get_name(enum bootflow_state_t state);

/**
 * bootflow_remove() - Remove a bootflow and free its memory
 *
 * This updates the linked lists containing the bootflow then frees it.
 *
 * @bflow: Bootflow to remove
 */
void bootflow_remove(struct bootflow *bflow);

#endif
