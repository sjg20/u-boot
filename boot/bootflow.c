// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <env.h>
#include <malloc.h>
#include <sort.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>

/* error codes used to signal running out of things */
enum {
	BF_NO_MORE_METHODS	= -ENOTTY,
	BF_NO_MORE_PARTS	= -ESHUTDOWN,
	BF_NO_MORE_DEVICES	= -ENODEV,
};

static const char *const bootflow_state[BOOTFLOWST_COUNT] = {
	"base",
	"media",
	"part",
	"fs",
	"file",
	"ready",
};

const char *bootflow_state_get_name(enum bootflow_state_t state)
{
	if (state < 0 || state >= BOOTFLOWST_COUNT)
		return "?";

	return bootflow_state[state];
}

int bootflow_first_glob(struct bootflow **bflowp)
{
	struct bootdev_state *state;
	int ret;

	ret = bootdev_get_state(&state);
	if (ret)
		return ret;

	if (list_empty(&state->glob_head))
		return -ENOENT;

	*bflowp = list_first_entry(&state->glob_head, struct bootflow,
				   glob_node);

	return 0;
}

int bootflow_next_glob(struct bootflow **bflowp)
{
	struct bootdev_state *state;
	struct bootflow *bflow = *bflowp;
	int ret;

	ret = bootdev_get_state(&state);
	if (ret)
		return ret;

	*bflowp = NULL;

	if (list_is_last(&bflow->glob_node, &state->glob_head))
		return -ENOENT;

	*bflowp = list_entry(bflow->glob_node.next, struct bootflow, glob_node);

	return 0;
}

void bootflow_iter_init(struct bootflow_iter *iter, int flags)
{
	memset(iter, '\0', sizeof(*iter));
	iter->flags = flags;
}

void bootflow_iter_uninit(struct bootflow_iter *iter)
{
	free(iter->dev_order);
}

static void bootflow_iter_set_dev(struct bootflow_iter *iter,
				  struct udevice *dev)
{
	iter->dev = dev;
	if ((iter->flags & (BOOTFLOWF_SHOW | BOOTFLOWF_SINGLE_DEV)) ==
	    BOOTFLOWF_SHOW) {
		if (dev)
			printf("Scanning bootdev '%s':\n", dev->name);
		else
			printf("No more bootdevs\n");
	}
}

/**
 * h_cmp_bootdev() - Compare two bootdevs to find out which should go first
 *
 * @v1: struct udevice * of first bootdev device
 * @v2: struct udevice * of second bootdev device
 * @return sort order (<0 if dev1 < dev2, ==0 if equal, >0 if dev1 > dev2)
 */
static int h_cmp_bootdev(const void *v1, const void *v2)
{
	const struct udevice *dev1 = *(struct udevice **)v1;
	const struct udevice *dev2 = *(struct udevice **)v2;
	const struct bootdev_uc_plat *ucp1 = dev_get_uclass_plat(dev1);
	const struct bootdev_uc_plat *ucp2 = dev_get_uclass_plat(dev2);

	if (dev_seq(dev1) != dev_seq(dev2))
		return dev_seq(dev1) - dev_seq(dev2);
	return ucp1->prio - ucp2->prio;
}

/**
 * find_bootdev_by_target() - Convert a target string to a bootdev device
 *
 * Looks up a target name to find the associated bootdev. For example, if the
 * target name is "mmc2", this will find a bootdev for an mmc device whose
 * sequence number is 2.
 *
 * @target: Target string to convert, e.g. "mmc2"
 * @devp: Returns bootdev device corresponding to that boot target
 * @return 0 if OK, -EINVAL if the target name (e.g. "mmc") does not refer to a
 *	uclass, -ENOENT if no bootdev for that media has the sequence number
 *	(e.g. 2)
 */
static int find_bootdev_by_target(const char *target, struct udevice **devp)
{
	struct udevice *media;
	struct uclass *uc;
	enum uclass_id id;
	int seq, len;

	seq = trailing_strtoln_len(target, NULL, &len);
	id = uclass_get_by_name_len(target, len);
	if (id == UCLASS_INVALID) {
		log_warning("Unknown uclass '%s' in boot_targets\n", target);
		return -EINVAL;
	}

	/* Iterate through devices in the media uclass (e.g. UCLASS_MMC) */
	uclass_id_foreach_dev(id, media, uc) {
		struct udevice *bdev;
		int ret;

		if (dev_seq(media) != seq)
			continue;

		ret = device_find_first_child_by_uclass(media, UCLASS_BOOTDEV,
							&bdev);
		if (!ret) {
			*devp = bdev;
			return 0;
		}
	}
	log_warning("Unknown seq %d for uclass '%s' in boot_targets\n",
		    seq, target);

	return -ENOENT;
}

/**
 * setup_order() - Set up the ordering of bootdevs to scan
 *
 * This sets up the ordering information in @iter, based on the priority of each
 * bootdev.
 *
 * If a single device is requested, no ordering is needed
 *
 * @iter: Iterator to update with the order
 * @dev: *devp is NULL to scan all, otherwise this is the (single) device to
 *	scan. Returns the first device to use
 * @return 0 if OK, -ENOENT if no bootdevs, -ENOMEM if out of memory, other -ve
 *	on other error
 */
static int setup_order(struct bootflow_iter *iter, struct udevice **devp)
{
	struct udevice *bootstd, *dev = *devp, **order;
	const char **target;
	int upto, i;
	int count;
	int ret;

	ret = uclass_first_device_err(UCLASS_BOOTSTD, &bootstd);
	if (ret)
		return log_msg_ret("std", ret);

	/* Handle scanning a single device */
	if (dev) {
		iter->flags |= BOOTFLOWF_SINGLE_DEV;
		return 0;
	}

	count = uclass_id_count(UCLASS_BOOTDEV);
	if (!count)
		return log_msg_ret("count", -ENOENT);

	order = calloc(count, sizeof(struct udevice *));
	if (!order)
		return log_msg_ret("order", -ENOMEM);

	/*
	 * Get a list of bootdevs, in seq order (i.e. using aliases). There may
	 * be gaps so try to count up high enough to find them all.
	 */
	for (i = 0, upto = 0; upto < count && i < 20 + count * 2; i++) {
		ret = uclass_find_device_by_seq(UCLASS_BOOTDEV, i, &dev);
		if (!ret)
			order[upto++] = dev;
	}
	if (upto != count)
		log_warning("Expected %d bootdevs, found %d using aliases\n",
			    count, upto);
	count = upto;

	target = bootstd_get_order(bootstd);
	if (target) {
		for (i = 0; target[i]; i++) {
			ret = find_bootdev_by_target(target[i], &dev);
			if (!ret) {
				if (i == count) {
					log_warning("Expected at most %d bootdevs, but overflowed with boot_target '%s'\n",
						    count, target[i]);
					break;
				}
				order[i++] = dev;
			}
		}
		count = i;
		free(target);
		if (!count) {
			free(order);
			return log_msg_ret("targ", -ENOMEM);
		}
	} else {
		/* sort them into priorty order */
		qsort(order, count, sizeof(struct udevice *), h_cmp_bootdev);
	}

	iter->dev_order = order;
	iter->num_devs = count;
	iter->cur_dev = 0;

	/* Use 'boot_targets' environment variable if available */
	dev = *order;
	ret = device_probe(dev);
	if (ret)
		return log_msg_ret("probe", ret);
	*devp = dev;

	return 0;
}

/**
 * iter_incr() - Move to the next item (method, part, bootdev)
 *
 * @return 0 if OK, BF_NO_MORE_DEVICES if there are no more bootdevs
 */
static int iter_incr(struct bootflow_iter *iter)
{
	struct udevice *dev;
	int ret;

	if (iter->err == BF_NO_MORE_DEVICES)
		return BF_NO_MORE_DEVICES;

	if (iter->err != BF_NO_MORE_PARTS && iter->err != BF_NO_MORE_METHODS) {
		/* Get the next boothmethod */
		ret = uclass_next_device_err(&iter->method);
		if (!ret)
			return 0;
	}

	/* No more bootmeths; start at the first one, and... */
	ret = uclass_first_device_err(UCLASS_BOOTMETH, &iter->method);
	if (ret)  /* should not happen, but just in case */
		return BF_NO_MORE_DEVICES;

	if (iter->err != BF_NO_MORE_PARTS) {
		/* ...select next partition  */
		if (++iter->part <= iter->max_part)
			return 0;
	}

	/* No more partitions; start at the first one and...*/
	iter->part = 0;

	/*
	 * Note: as far as we know, there is no partition table on the next
	 * bootdev, so set max_part to 0 until we discover otherwise. See
	 * bootdev_find_in_blk() for where this is set.
	 */
	iter->max_part = 0;

	/* ...select next bootdev */
	if (iter->flags & BOOTFLOWF_SINGLE_DEV) {
		ret = -ENOENT;
	} else if (++iter->cur_dev == iter->num_devs) {
		ret = -ENOENT;
		bootflow_iter_set_dev(iter, NULL);
	} else {
		dev = iter->dev_order[iter->cur_dev];
		ret = device_probe(dev);
		if (!log_msg_ret("probe", ret))
			bootflow_iter_set_dev(iter, dev);
	}

	/* if there are no more bootdevs, give up */
	if (ret)
		return log_msg_ret("next", BF_NO_MORE_DEVICES);

	return 0;
}

/**
 * bootflow_check() - Check if a bootflow can be obtained
 *
 * @iter: Provides part, method to get
 * @bflow: Bootflow to update on success
 * @return 0 if OK, -ENOSYS if there is no bootflow support on this device,
 *	BF_NO_MORE_PARTS if there are no more partitions on bootdev
 */
static int bootflow_check(struct bootflow_iter *iter, struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	dev = iter->dev;
	ret = bootdev_get_bootflow(dev, iter, bflow);

	/* If we got a valid bootflow, return it */
	if (!ret) {
		log_debug("Bootdevice '%s' part %d method '%s': Found bootflow\n",
			  dev->name, iter->part, iter->method->name);
		return 0;
	}

	/* Unless there is nothing more to try, move to the next device */
	else if (ret != BF_NO_MORE_PARTS && ret != -ENOSYS) {
		log_debug("Bootdevice '%s' part %d method '%s': Error %d\n",
			  dev->name, iter->part, iter->method->name, ret);
		/*
		 * For 'all' we return all bootflows, even
		 * those with errors
		 */
		if (iter->flags & BOOTFLOWF_ALL)
			return log_msg_ret("all", ret);
	}
	if (ret)
		return log_msg_ret("check", ret);

	return 0;
}

int bootflow_scan_bootdev(struct udevice *dev, struct bootflow_iter *iter,
			  int flags, struct bootflow *bflow)
{
	int ret;

	bootflow_iter_init(iter, flags);

	ret = setup_order(iter, &dev);
	if (ret)
		return log_msg_ret("order", -ENODEV);
	bootflow_iter_set_dev(iter, dev);

	/* Find the first bootmeth (there must be at least one!) */
	ret = uclass_first_device_err(UCLASS_BOOTMETH, &iter->method);
	if (ret)
		return log_msg_ret("meth", -ENODEV);

	ret = bootflow_check(iter, bflow);
	if (ret) {
		if (ret != BF_NO_MORE_PARTS && ret != -ENOSYS) {
			if (iter->flags & BOOTFLOWF_ALL)
				return log_msg_ret("all", ret);
		}
		iter->err = ret;
		ret = bootflow_scan_next(iter, bflow);
		if (ret)
			return log_msg_ret("get", ret);
	}

	return 0;
}

int bootflow_scan_first(struct bootflow_iter *iter, int flags,
			struct bootflow *bflow)
{
	return bootflow_scan_bootdev(NULL, iter, flags, bflow);
}

int bootflow_scan_next(struct bootflow_iter *iter, struct bootflow *bflow)
{
	int ret;

	do {
		ret = iter_incr(iter);
		if (ret == BF_NO_MORE_DEVICES)
			return log_msg_ret("done", ret);

		if (!ret) {
			ret = bootflow_check(iter, bflow);
			if (!ret)
				return 0;
			iter->err = ret;
			if (ret != BF_NO_MORE_PARTS && ret != -ENOSYS) {
				if (iter->flags & BOOTFLOWF_ALL)
					return log_msg_ret("all", ret);
			}
		} else {
			iter->err = ret;
		}

	} while (1);
}

void bootflow_free(struct bootflow *bflow)
{
	free(bflow->name);
	free(bflow->subdir);
	free(bflow->fname);
	free(bflow->buf);
}

void bootflow_remove(struct bootflow *bflow)
{
	list_del(&bflow->bm_node);
	list_del(&bflow->glob_node);

	bootflow_free(bflow);
	free(bflow);
}

int bootflow_boot(struct bootflow *bflow)
{
	int ret;

	if (bflow->state != BOOTFLOWST_READY)
		return log_msg_ret("load", -EPROTO);

	ret = bootmeth_boot(bflow->method, bflow);
	if (ret)
		return log_msg_ret("method", ret);

	if (ret)
		return log_msg_ret("boot", ret);

	/*
	 * internal error, should not get here since we should have booted
	 * something or returned an error
	 */

	return log_msg_ret("end", -EFAULT);
}
