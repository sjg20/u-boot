/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootmethod_h
#define __bootmethod_h

struct bootflow {
};

struct bootmethod_iter {
	int flags;
	int seq;
	struct udevice *dev;
};

/**
 * enum bootflow_flags_t - flags for the bootflow
 *
 * @BOOTFLOWF_FIXED: Only used fixed/internal media
 */
enum bootflow_flags_t {
	BOOTFLOWF_FIXED		= 1 << 0,
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

#define bootmethod_get_ops(dev)        ((struct pch_ops *)(dev)->driver->ops)

/**
 * bootmethod_get_bootflow() - get a bootflow
 *
 * @dev:	Bootflow device to check
 * @seq:	Sequence number of bootflow to read (0 for first)
 * @bflow:	Returns bootflow if found
 * @return 0 if OK, -ve on error (e.g. there is no SPI base)
 */
int (*bootmethod_get_bootflow)(struct udevice *dev, int seq,
			       struct bootflow *bflow);

/**
 * bootmethod_first_bootflow() - find the first bootflow
 *
 * This works through the available bootmethod devices until it finds one that
 * can supply a bootflow. It then returns that
 *
 * @iter:	Place to store private info (inited by this call)
 * @flags:	Flags for bootmethod (enum bootflow_flags_t)
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootmethod_first_bootflow(struct bootmethod_iter *iter, int flags,
			      struct bootflow *bflow);

/**
 * bootmethod_next_bootflow() - find the next bootflow
 *
 * This works through the available bootmethod devices until it finds one that
 * can supply a bootflow. It then returns that bootflow
 *
 * @iter:	Private info (as set up by bootmethod_first_bootflow())
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, -ve on error
 */
int bootmethod_next_bootflow(struct bootmethod_iter *iter,
			     struct bootflow *bflow);

#endif
