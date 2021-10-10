// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <dm.h>
#include <malloc.h>
#include <sort.h>
#include <dm/device-internal.h>

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
 * @v1: struct udevice * of first device
 * @v2: struct udevice * of second device
 * @return sort order (<0 if dev1 < dev2, ==0 if equal, >0 if dev1 > dev2)
 */
static int h_cmp_bootdev(const void *v1, const void *v2)
{
	const struct udevice *dev1 = *(struct udevice **)v1;
	const struct udevice *dev2 = *(struct udevice **)v2;
	const struct bootdev_uc_plat *ucp1 = dev_get_uclass_plat(dev1);
	const struct bootdev_uc_plat *ucp2 = dev_get_uclass_plat(dev2);

	return ucp1->prio - ucp2->prio;
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
	struct udevice *dev = *devp, **order;
	struct uclass *uc;
	int count;
	int ret;
	int i;

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

	/* Get a list of bootdevs */
	i = 0;
	uclass_id_foreach_dev(UCLASS_BOOTDEV, dev, uc)
		order[i++] = dev;

	/* sort them into priorty order */
	qsort(order, count, sizeof(struct udevice *), h_cmp_bootdev);

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
		return log_msg_ret("order", ret);
	bootflow_iter_set_dev(iter, dev);

	/* Find the first bootmeth (there must be at least one!) */
	ret = uclass_first_device_err(UCLASS_BOOTMETH, &iter->method);
	if (ret)
		return log_msg_ret("meth", ret);

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
