// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootstd.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <part.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>

enum {
	/*
	 * Set some sort of limit on the number of partitions a bootdev can
	 * have. Note that for disks this limits the partitions numbers that
	 * are scanned to 1..MAX_BOOTFLOWS_PER_BOOTDEV
	 */
	MAX_PART_PER_BOOTDEV		= 30,
};

int bootdev_add_bootflow(struct bootflow *bflow)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(bflow->dev);
	struct bootstd_priv *std;
	struct bootflow *new;
	int ret;

	assert(bflow->dev);
	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;

	new = malloc(sizeof(*bflow));
	if (!new)
		return log_msg_ret("bflow", -ENOMEM);
	memcpy(new, bflow, sizeof(*bflow));

	list_add_tail(&new->glob_node, &std->glob_head);
	list_add_tail(&new->bm_node, &ucp->bootflow_head);

	return 0;
}

int bootdev_first_bootflow(struct udevice *dev, struct bootflow **bflowp)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	if (list_empty(&ucp->bootflow_head))
		return -ENOENT;

	*bflowp = list_first_entry(&ucp->bootflow_head, struct bootflow,
				   bm_node);

	return 0;
}

int bootdev_next_bootflow(struct bootflow **bflowp)
{
	struct bootflow *bflow = *bflowp;
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(bflow->dev);

	*bflowp = NULL;

	if (list_is_last(&bflow->bm_node, &ucp->bootflow_head))
		return -ENOENT;

	*bflowp = list_entry(bflow->bm_node.next, struct bootflow, bm_node);

	return 0;
}

int bootdev_bind(struct udevice *parent, const char *drv_name, const char *name,
		 struct udevice **devp)
{
	struct udevice *dev;
	char dev_name[30];
	char *str;
	int ret;

	snprintf(dev_name, sizeof(dev_name), "%s.%s", parent->name, name);
	str = strdup(dev_name);
	if (!str)
		return -ENOMEM;
	ret = device_bind_driver(parent, drv_name, str, &dev);
	if (ret)
		return ret;
	device_set_name_alloced(dev);
	*devp = dev;

	return 0;
}

int bootdev_find_in_blk(struct udevice *dev, struct udevice *blk,
			struct bootflow_iter *iter, struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(blk);
	struct disk_partition info;
	char partstr[20];
	char name[60];
	int ret;

	/* Sanity check */
	if (iter->part >= MAX_PART_PER_BOOTDEV)
		return log_msg_ret("max", -ESHUTDOWN);

	bflow->blk = blk;
	if (iter->part)
		snprintf(partstr, sizeof(partstr), "part_%x", iter->part);
	else
		strcpy(partstr, "whole");
	snprintf(name, sizeof(name), "%s.%s", dev->name, partstr);
	bflow->name = strdup(name);
	if (!bflow->name)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_BASE;
	bflow->part = iter->part;

	/*
	 * partition numbers start at 0 so this cannot succeed, but it can tell
	 * us whether there is valid media there
	 */
	ret = part_get_info(desc, iter->part, &info);
	if (!iter->part && ret == -ENOENT)
		ret = 0;

	/*
	 * This error indicates the media is not present. Otherwise we just
	 * blindly scan the next partition. We could be more intelligent here
	 * and check which partition numbers actually exist.
	 */
	if (ret == -EOPNOTSUPP)
		ret = -ESHUTDOWN;
	else
		bflow->state = BOOTFLOWST_MEDIA;
	if (ret)
		return log_msg_ret("part", ret);

	/*
	 * Currently we don't get the number of partitions, so just
	 * assume a large number
	 */
	iter->max_part = MAX_PART_PER_BOOTDEV;

	if (iter->part) {
		ret = fs_set_blk_dev_with_part(desc, bflow->part);
		bflow->state = BOOTFLOWST_PART;

		/* Use an #ifdef due to info.sys_ind */
#ifdef CONFIG_DOS_PARTITION
		log_debug("%s: Found partition %x type %x fstype %d\n",
			  blk->name, bflow->part, info.sys_ind,
			  ret ? -1 : fs_get_type());
#endif
		if (ret)
			return log_msg_ret("fs", ret);
		bflow->state = BOOTFLOWST_FS;
	}

	return 0;
}

void bootdev_list(bool probe)
{
	struct udevice *dev;
	int ret;
	int i;

	printf("Seq  Probed  Status  Uclass    Name\n");
	printf("---  ------  ------  --------  ------------------\n");
	if (probe)
		ret = uclass_first_device_err(UCLASS_BOOTDEV, &dev);
	else
		ret = uclass_find_first_device(UCLASS_BOOTDEV, &dev);
	for (i = 0; dev; i++) {
		printf("%3x   [ %c ]  %6s  %-9.9s %s\n", dev_seq(dev),
		       device_active(dev) ? '+' : ' ',
		       ret ? simple_itoa(ret) : "OK",
		       dev_get_uclass_name(dev_get_parent(dev)), dev->name);
		if (probe)
			ret = uclass_next_device_err(&dev);
		else
			ret = uclass_find_next_device(&dev);
	}
	printf("---  ------  ------  --------  ------------------\n");
	printf("(%d bootdev%s)\n", i, i != 1 ? "s" : "");
}

int bootdev_setup_for_dev(struct udevice *parent, const char *drv_name)
{
	struct udevice *bdev;
	int ret;

	ret = device_find_first_child_by_uclass(parent, UCLASS_BOOTDEV,
						&bdev);
	if (ret) {
		if (ret != -ENODEV) {
			log_debug("Cannot access bootdev device\n");
			return ret;
		}

		ret = bootdev_bind(parent, drv_name, "bootdev", &bdev);
		if (ret) {
			log_debug("Cannot create bootdev device\n");
			return ret;
		}
	}

	return 0;
}

int bootdev_setup_sibling_blk(struct udevice *blk, const char *drv_name)
{
	struct udevice *parent, *dev;
	char dev_name[50];
	int ret;

	snprintf(dev_name, sizeof(dev_name), "%s.%s", blk->name, "bootdev");

	parent = dev_get_parent(blk);
	ret = device_find_child_by_name(parent, dev_name, &dev);
	if (ret) {
		char *str;

		if (ret != -ENODEV) {
			log_debug("Cannot access bootdev device\n");
			return ret;
		}
		str = strdup(dev_name);
		if (!str)
			return -ENOMEM;

		ret = device_bind_driver(parent, drv_name, str, &dev);
		if (ret) {
			log_debug("Cannot create bootdev device\n");
			return ret;
		}
		device_set_name_alloced(dev);
	}

	return 0;
}

int bootdev_get_sibling_blk(struct udevice *dev, struct udevice **blkp)
{
	struct udevice *parent = dev_get_parent(dev);
	struct udevice *blk;
	int ret, len;
	char *p;

	if (device_get_uclass_id(dev) != UCLASS_BOOTDEV)
		return -EINVAL;

	/* This should always work if bootdev_setup_sibling_blk() was used */
	p = strstr(dev->name, ".bootdev");
	if (!p)
		return log_msg_ret("str", -EINVAL);

	len = p - dev->name;
	ret = device_find_child_by_namelen(parent, dev->name, len, &blk);
	if (ret)
		return log_msg_ret("find", ret);
	*blkp = blk;

	return 0;
}

int bootdev_unbind_dev(struct udevice *parent)
{
	struct udevice *dev;
	int ret;

	ret = device_find_first_child_by_uclass(parent, UCLASS_BOOTDEV, &dev);
	if (!ret) {
		ret = device_remove(dev, DM_REMOVE_NORMAL);
		if (ret)
			return log_msg_ret("rem", ret);
		ret = device_unbind(dev);
		if (ret)
			return log_msg_ret("unb", ret);
	}

	return 0;
}

/**
 * bootdev_find_by_label() - Convert a label string to a bootdev device
 *
 * Looks up a label name to find the associated bootdev. For example, if the
 * label name is "mmc2", this will find a bootdev for an mmc device whose
 * sequence number is 2.
 *
 * @label: Label string to convert, e.g. "mmc2"
 * @devp: Returns bootdev device corresponding to that boot label
 * @return 0 if OK, -EINVAL if the label name (e.g. "mmc") does not refer to a
 *	uclass, -ENOENT if no bootdev for that media has the sequence number
 *	(e.g. 2)
 */
int bootdev_find_by_label(const char *label, struct udevice **devp)
{
	struct udevice *media;
	struct uclass *uc;
	enum uclass_id id;
	int seq, len;

	seq = trailing_strtoln_len(label, NULL, &len);
	id = uclass_get_by_name_len(label, len);
	if (id == UCLASS_INVALID) {
		log_warning("Unknown uclass '%s' in label\n", label);
		return -EINVAL;
	}

	/* Iterate through devices in the media uclass (e.g. UCLASS_MMC) */
	uclass_id_foreach_dev(id, media, uc) {
		struct udevice *bdev;
		int ret;

		/* if there is no seq, match anything */
		if (seq != -1 && dev_seq(media) != seq)
			continue;

		ret = device_find_first_child_by_uclass(media, UCLASS_BOOTDEV,
							&bdev);
		if (!ret) {
			*devp = bdev;
			return 0;
		}
	}
	log_warning("Unknown seq %d for label '%s'\n", seq, label);

	return -ENOENT;
}

int bootdev_find_by_any(const char *name, struct udevice **devp)
{
	struct udevice *dev;
	int ret, seq;
	char *endp;

	seq = simple_strtol(name, &endp, 16);

	/* Select by name, label or number */
	if (*endp) {
		ret = uclass_get_device_by_name(UCLASS_BOOTDEV, name, &dev);
		if (ret == -ENODEV) {
			ret = bootdev_find_by_label(name, &dev);
			if (ret) {
				printf("Cannot find bootdev '%s' (err=%d)\n",
				       name, ret);
				return ret;
			}
			ret = device_probe(dev);
		}
		if (ret) {
			printf("Cannot probe bootdev '%s' (err=%d)\n", name,
			       ret);
			return ret;
		}
	} else {
		ret = uclass_get_device_by_seq(UCLASS_BOOTDEV, seq, &dev);
	}
	if (ret) {
		printf("Cannot find '%s' (err=%d)\n", name, ret);
		return ret;
	}

	*devp = dev;

	return 0;
}

int bootdev_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			 struct bootflow *bflow)
{
	const struct bootdev_ops *ops = bootdev_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;
	memset(bflow, '\0', sizeof(*bflow));
	bflow->dev = dev;
	bflow->method = iter->method;
	bflow->state = BOOTFLOWST_BASE;

	return ops->get_bootflow(dev, iter, bflow);
}

void bootdev_clear_bootflows(struct udevice *dev)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	while (!list_empty(&ucp->bootflow_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&ucp->bootflow_head, struct bootflow,
					 bm_node);
		/* later bootflow_remove(bflow); */
	}
}

static int bootdev_post_bind(struct udevice *dev)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	INIT_LIST_HEAD(&ucp->bootflow_head);

	return 0;
}

static int bootdev_pre_unbind(struct udevice *dev)
{
	bootdev_clear_bootflows(dev);

	return 0;
}

UCLASS_DRIVER(bootdev) = {
	.id		= UCLASS_BOOTDEV,
	.name		= "bootdev",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_plat_auto	= sizeof(struct bootdev_uc_plat),
	.post_bind	= bootdev_post_bind,
	.pre_unbind	= bootdev_pre_unbind,
};
