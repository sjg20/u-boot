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
	struct bootstd_priv *std;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;

	if (list_empty(&std->glob_head))
		return -ENOENT;

	*bflowp = list_first_entry(&std->glob_head, struct bootflow,
				   glob_node);

	return 0;
}

int bootflow_next_glob(struct bootflow **bflowp)
{
	struct bootstd_priv *std;
	struct bootflow *bflow = *bflowp;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;

	*bflowp = NULL;

	if (list_is_last(&bflow->glob_node, &std->glob_head))
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
	free(iter->method_order);
}

int bootflow_iter_drop_bootmeth(struct bootflow_iter *iter,
				const struct udevice *bmeth)
{
	/* We only support disabling the current bootmeth */
	if (bmeth != iter->method || iter->cur_method >= iter->num_methods ||
	    iter->method_order[iter->cur_method] != bmeth)
		return -EINVAL;

	memmove(&iter->method_order[iter->cur_method],
		&iter->method_order[iter->cur_method + 1],
		(iter->num_methods - iter->cur_method - 1) * sizeof(void *));

	iter->num_methods--;

	return 0;
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
	int diff;

	/* Use priority first */
	diff = ucp1->prio - ucp2->prio;
	if (diff)
		return diff;

	/* Fall back to seq for devices of the same priority */
	diff = dev_seq(dev1) - dev_seq(dev2);

	return diff;
}

/**
 * setup_order() - Set up the ordering of bootdevs to scan
 *
 * This sets up the ordering information in @iter, based on the priority of each
 * bootdev and the bootdev-order property in the bootstd node
 *
 * If a single device is requested, no ordering is needed
 *
 * @iter: Iterator to update with the order
 * @devp: On entry, *devp is NULL to scan all, otherwise this is the (single)
 *	device to scan. Returns the first device to use, which is the passed-in
 *	@devp if it was non-NULL
 * @return 0 if OK, -ENOENT if no bootdevs, -ENOMEM if out of memory, other -ve
 *	on other error
 */
static int setup_bootdev_order(struct bootflow_iter *iter,
			       struct udevice **devp)
{
	struct udevice *bootstd, *dev = *devp, **order;
	const char *const *labels;
	int upto, i;
	int count;
	int ret;

	ret = uclass_first_device_err(UCLASS_BOOTSTD, &bootstd);
	if (ret) {
		log_err("Missing bootstd device\n");
		return log_msg_ret("std", ret);
	}

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
	log_debug("Found %d bootdevs\n", count);
	if (upto != count)
		log_warning("Expected %d bootdevs, found %d using aliases\n",
			    count, upto);
	count = upto;

	labels = bootstd_get_bootdev_order(bootstd);
	if (labels) {
		upto = 0;
		for (i = 0; labels[i]; i++) {
			ret = bootdev_find_by_label(labels[i], &dev);
			if (!ret) {
				if (upto == count) {
					log_warning("Expected at most %d bootdevs, but overflowed with boot_target '%s'\n",
						    count, labels[i]);
					break;
				}
				order[upto++] = dev;
			}
		}
		count = upto;
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

	dev = *order;
	ret = device_probe(dev);
	if (ret)
		return log_msg_ret("probe", ret);
	*devp = dev;

	return 0;
}

/**
 * setup_bootmeth_order() - Set up the ordering of bootmeths to scan
 *
 * This sets up the ordering information in @iter, based on the selected
 * ordering of the bootmethds in bootstd_priv->bootmeth_order. If there is no
 * ordering there, then all bootmethods are added
 *
 * @iter: Iterator to update with the order
 * @return 0 if OK, -ENOENT if no bootdevs, -ENOMEM if out of memory, other -ve
 *	on other error
 */
static int setup_bootmeth_order(struct bootflow_iter *iter)
{
	struct bootstd_priv *std;
	struct udevice **order;
	int count;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;

	/* Create an array large enough */
	count = std->bootmeth_count ? std->bootmeth_count :
		uclass_id_count(UCLASS_BOOTMETH);
	if (!count)
		return log_msg_ret("count", -ENOENT);

	order = calloc(count, sizeof(struct udevice *));
	if (!order)
		return log_msg_ret("order", -ENOMEM);

	/* If we have an ordering, copy it */
	if (std->bootmeth_count) {
		memcpy(order, std->bootmeth_order,
		       count * sizeof(struct bootmeth *));
	} else {
		struct udevice *dev;
		int i, upto;

		/*
		 * Get a list of bootmethods, in seq order (i.e. using aliases).
		 * There may be gaps so try to count up high enough to find them
		 * all.
		 */
		for (i = 0, upto = 0; upto < count && i < 20 + count * 2; i++) {
			ret = uclass_get_device_by_seq(UCLASS_BOOTMETH, i,
						       &dev);
			if (!ret)
				order[upto++] = dev;
		}
		count = upto;
	}

	iter->method_order = order;
	iter->num_methods = count;
	iter->cur_method = 0;

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

	if (iter->err != BF_NO_MORE_PARTS) {
		/* Get the next boothmethod */
		if (++iter->cur_method < iter->num_methods) {
			iter->method = iter->method_order[iter->cur_method];
			return 0;
		}
	}

	/* No more bootmeths; start at the first one, and... */
	iter->cur_method = 0;
	iter->method = iter->method_order[iter->cur_method];

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
 * @iter: Provides part, bootmeth to use
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

	ret = setup_bootdev_order(iter, &dev);
	if (ret)
		return log_msg_ret("obdev", -ENODEV);
	bootflow_iter_set_dev(iter, dev);

	ret = setup_bootmeth_order(iter);
	if (ret)
		return log_msg_ret("obmeth", -ENODEV);

	/* Find the first bootmeth (there must be at least one!) */
	iter->method = iter->method_order[iter->cur_method];

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
		return log_msg_ret("boot", ret);

	/*
	 * internal error, should not get here since we should have booted
	 * something or returned an error
	 */

	return log_msg_ret("end", -EFAULT);
}

int bootflow_run_boot(struct bootflow_iter *iter, struct bootflow *bflow)
{
	int ret;

	printf("** Booting bootflow '%s'\n", bflow->name);
	ret = bootflow_boot(bflow);
	switch (ret) {
	case -EPROTO:
		printf("Bootflow not loaded (state '%s')\n",
		       bootflow_state_get_name(bflow->state));
		break;
	case -ENOSYS:
		printf("Boot method '%s' not supported\n", bflow->method->name);
		break;
	case -ENOTSUPP:
		/* Disable this bootflow for this iteration */
		if (iter) {
			int ret2;

			ret2 = bootflow_iter_drop_bootmeth(iter, bflow->method);
			if (!ret2) {
				printf("Boot method '%s' failed and will not be retried\n",
				       bflow->method->name);
			}
		}

		break;
	default:
		printf("Boot failed (err=%d)\n", ret);
		break;
	}

	return ret;
}

int bootflow_iter_uses_blk_dev(const struct bootflow_iter *iter)
{
	const struct udevice *media = dev_get_parent(iter->dev);
	enum uclass_id id = device_get_uclass_id(media);

	log_debug("uclass %d: %s\n", id, uclass_get_name(id));
	if (id != UCLASS_ETH && id != UCLASS_BOOTSTD)
		return 0;

	return -ENOTSUPP;
}

int bootflow_iter_uses_network(const struct bootflow_iter *iter)
{
	const struct udevice *media = dev_get_parent(iter->dev);
	enum uclass_id id = device_get_uclass_id(media);

	log_debug("uclass %d: %s\n", id, uclass_get_name(id));
	if (id == UCLASS_ETH)
		return 0;

	return -ENOTSUPP;
}

int bootflow_iter_uses_system(const struct bootflow_iter *iter)
{
	const struct udevice *media = dev_get_parent(iter->dev);
	enum uclass_id id = device_get_uclass_id(media);

	log_debug("uclass %d: %s\n", id, uclass_get_name(id));
	if (id == UCLASS_BOOTSTD)
		return 0;

	return -ENOTSUPP;
}
