// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootdev.h>
#include <bootflow.h>
#include <dm.h>
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
	struct bootdev_state *state;
	struct bootflow *new;
	int ret;

	assert(bflow->dev);
	ret = bootdev_get_state(&state);
	if (ret)
		return ret;

	new = malloc(sizeof(*bflow));
	if (!new)
		return log_msg_ret("bflow", -ENOMEM);
	memcpy(new, bflow, sizeof(*bflow));

	list_add_tail(&new->glob_node, &state->glob_head);
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
	printf("(%d device%s)\n", i, i != 1 ? "s" : "");
}

int bootdev_setup_for_dev(struct udevice *parent, const char *drv_name)
{
	struct udevice *bm;
	int ret;

	ret = device_find_first_child_by_uclass(parent, UCLASS_BOOTDEV,
						&bm);
	if (ret) {
		if (ret != -ENODEV) {
			log_debug("Cannot access bootdev device\n");
			return ret;
		}

		ret = bootdev_bind(parent, drv_name, "bootdev", &bm);
		if (ret) {
			log_debug("Cannot create bootdev device\n");
			return ret;
		}
	}

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
