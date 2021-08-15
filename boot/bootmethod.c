// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootmethod.h>
#include <distro.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <part.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>

enum {
	/*
	 * Set some sort of limit on the number of bootflows a bootmethod can
	 * return
	 */
	MAX_BOOTFLOWS_PER_BOOTMETHOD	= 10,
};

static const char *const bootmethod_state[BOOTFLOWST_COUNT] = {
	"base",
	"media",
	"part",
	"fs",
	"file",
	"loaded",
};

static const char *const bootmethod_type[BOOTFLOWT_COUNT] = {
	"distro-boot",
};

int bootmethod_get_state(struct bootflow_state **statep)
{
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_BOOTMETHOD, &uc);
	if (ret)
		return ret;
	*statep = uclass_get_priv(uc);

	return 0;
}

const char *bootmethod_state_get_name(enum bootflow_state_t state)
{
	if (state < 0 || state >= BOOTFLOWST_COUNT)
		return "?";

	return bootmethod_state[state];
}

const char *bootmethod_type_get_name(enum bootflow_type_t type)
{
	if (type < 0 || type >= BOOTFLOWT_COUNT)
		return "?";

	return bootmethod_type[type];
}

void bootflow_free(struct bootflow *bflow)
{
	free(bflow->fname);
	free(bflow->name);
	free(bflow->buf);
}

void bootflow_remove(struct bootflow *bflow)
{
	list_del(&bflow->bm_node);
	list_del(&bflow->glob_node);

	bootflow_free(bflow);
}

void bootmethod_clear_bootflows(struct udevice *dev)
{
	struct bootmethod_uc_priv *ucp = dev_get_uclass_priv(dev);

	while (!list_empty(&ucp->bootflow_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&ucp->bootflow_head, struct bootflow,
					 bm_node);
		bootflow_remove(bflow);
	}
}

void bootmethod_clear_glob(void)
{
	struct bootflow_state *state;

	if (bootmethod_get_state(&state))
		return;

	while (!list_empty(&state->glob_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&state->glob_head, struct bootflow,
					 glob_node);
		bootflow_remove(bflow);
	}
}

int bootmethod_add_bootflow(struct bootflow *bflow)
{
	struct bootmethod_uc_priv *ucp = dev_get_uclass_priv(bflow->dev);
	struct bootflow_state *state;
	struct bootflow *new;
	int ret;

	ret = bootmethod_get_state(&state);
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

int bootmethod_first_bootflow(struct udevice *dev, struct bootflow **bflowp)
{
	struct bootmethod_uc_priv *ucp = dev_get_uclass_priv(dev);

	if (list_empty(&ucp->bootflow_head))
		return -ENOENT;

	*bflowp = list_first_entry(&ucp->bootflow_head, struct bootflow,
				   bm_node);

	return 0;
}

int bootmethod_next_bootflow(struct bootflow **bflowp)
{
	struct bootflow *bflow = *bflowp;
	struct bootmethod_uc_priv *ucp = dev_get_uclass_priv(bflow->dev);

	*bflowp = NULL;

	if (list_is_last(&bflow->bm_node, &ucp->bootflow_head))
		return -ENOENT;

	*bflowp = list_entry(bflow->bm_node.next, struct bootflow, bm_node);

	return 0;
}

int bootflow_first_glob(struct bootflow **bflowp)
{
	struct bootflow_state *state;
	int ret;

	ret = bootmethod_get_state(&state);
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
	struct bootflow_state *state;
	struct bootflow *bflow = *bflowp;
	int ret;

	ret = bootmethod_get_state(&state);
	if (ret)
		return ret;

	*bflowp = NULL;

	if (list_is_last(&bflow->glob_node, &state->glob_head))
		return -ENOENT;

	*bflowp = list_entry(bflow->glob_node.next, struct bootflow, glob_node);

	return 0;
}

int bootmethod_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;
	memset(bflow, '\0', sizeof(*bflow));

	return ops->get_bootflow(dev, seq, bflow);
}

static int next_bootflow(struct udevice *dev, int seq, struct bootflow *bflow)
{
	int ret;

	ret = bootmethod_get_bootflow(dev, seq, bflow);
	if (ret)
		return ret;

	return 0;
}

static void bootmethod_iter_set_dev(struct bootmethod_iter *iter,
				    struct udevice *dev)
{
	iter->dev = dev;
	if (iter->flags & BOOTFLOWF_SHOW) {
		if (dev)
			printf("Scanning bootmethod '%s':\n", dev->name);
		else
			printf("No more bootmethods\n");
	}
}

int bootmethod_scan_first_bootflow(struct bootmethod_iter *iter, int flags,
				   struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	iter->flags = flags;
	iter->seq = 0;
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &dev);
	if (ret)
		return ret;
	bootmethod_iter_set_dev(iter, dev);

	ret = bootmethod_scan_next_bootflow(iter, bflow);
	if (ret)
		return ret;

	return 0;
}

int bootmethod_scan_next_bootflow(struct bootmethod_iter *iter,
				  struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	do {
		dev = iter->dev;
		ret = next_bootflow(dev, iter->seq, bflow);

		/* If we got a valid bootflow, return it */
		if (!ret) {
			log_debug("Bootmethod '%s' seq %d: Found bootflow\n",
				  dev->name, iter->seq);
			iter->seq++;
			return 0;
		}

		/* If we got some other error, try the next partition */
		else if (ret != -ESHUTDOWN) {
			log_debug("Bootmethod '%s' seq %d: Error %d\n",
				  dev->name, iter->seq, ret);
			if (iter->seq++ == MAX_BOOTFLOWS_PER_BOOTMETHOD)
				/* fall through to next device */;
			else if (iter->flags & BOOTFLOWF_ALL)
				return log_msg_ret("all", ret);
			else
				continue;
		}

		/* we got to the end of that bootmethod, try the next */
		ret = uclass_next_device_err(&dev);
		bootmethod_iter_set_dev(iter, dev);

		/* if there are no more bootmethods, give up */
		if (ret)
			return ret;

		/* start at the beginning of this bootmethod */
		iter->seq = 0;
	} while (1);
}

int bootmethod_bind(struct udevice *parent, const char *drv_name,
		    const char *name, struct udevice **devp)
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
	*devp = dev;

	return 0;
}

int bootmethod_find_in_blk(struct udevice *dev, struct udevice *blk, int seq,
			   struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(blk);
	struct disk_partition info;
	char name[60];
	int partnum = seq + 1;
	int ret;

	if (seq >= MAX_BOOTFLOWS_PER_BOOTMETHOD)
		return -ESHUTDOWN;

	bflow->dev = dev;
	bflow->blk = blk;
	bflow->seq = seq;
	snprintf(name, sizeof(name), "%s.part_%x", dev->name, partnum);
	bflow->name = strdup(name);
	if (!bflow->name)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_BASE;
	ret = part_get_info(desc, partnum, &info);
	if (ret)
		return log_msg_ret("part", ret);

	bflow->state = BOOTFLOWST_PART;
	bflow->part = partnum;
	ret = fs_set_blk_dev_with_part(desc, partnum);
	log_debug("%s: Found partition %x type %x fstype %d\n", blk->name,
		  partnum, info.sys_ind, ret ? -1 : fs_get_type());
	if (ret)
		return log_msg_ret("fs", ret);

	bflow->state = BOOTFLOWST_FS;

	if (CONFIG_IS_ENABLED(BOOTMETHOD_DISTRO)) {
		ret = distro_boot_setup(desc, partnum, bflow);
		if (ret)
			return log_msg_ret("distro", ret);
	}

	return 0;
}

int bootflow_boot(struct bootflow *bflow)
{
	bool done = false;
	int ret;

	if (bflow->state != BOOTFLOWST_LOADED)
		return log_msg_ret("load", ret);

	switch (bflow->type) {
	case BOOTFLOWT_DISTRO:
		if (CONFIG_IS_ENABLED(BOOTMETHOD_DISTRO)) {
			done = true;
			ret = distro_boot(bflow);
		}
		break;
	case BOOTFLOWT_COUNT:
		break;
	}

	if (!done)
		return log_msg_ret("type", -ENOSYS);

	if (ret)
		return log_msg_ret("boot", ret);

	/*
	 * internal error, should not get here since we should have booted
	 * something or returned an error
	 */

	return log_msg_ret("end", -EFAULT);
}

void bootmethod_list(bool probe)
{
	struct udevice *dev;
	int ret;
	int i;

	printf("Seq  Probed  Status  Name\n");
	printf("---  ------  ------  ------------------\n");
	if (probe)
		ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &dev);
	else
		ret = uclass_find_first_device(UCLASS_BOOTMETHOD, &dev);
	for (i = 0; dev; i++) {
		printf("%3x   [ %c ]  %6s  %s\n", dev_seq(dev),
		       device_active(dev) ? '+' : ' ',
		       ret ? simple_itoa(ret) : "OK", dev->name);
		if (probe)
			ret = uclass_next_device_err(&dev);
		else
			ret = uclass_find_next_device(&dev);
	}
	printf("---  ------  ------  ------------------\n");
	printf("(%d device%s)\n", i, i != 1 ? "s" : "");
}

static int bootmethod_init(struct uclass *uc)
{
	struct bootflow_state *state = uclass_get_priv(uc);

	INIT_LIST_HEAD(&state->glob_head);

	return 0;
}

static int bootmethod_pre_probe(struct udevice *dev)
{
	struct bootmethod_uc_priv *ucp = dev_get_uclass_priv(dev);

	INIT_LIST_HEAD(&ucp->bootflow_head);

	return 0;
}

UCLASS_DRIVER(bootmethod) = {
	.id		= UCLASS_BOOTMETHOD,
	.name		= "bootmethod",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.priv_auto	= sizeof(struct bootflow_state),
	.per_device_auto	= sizeof(struct bootmethod_uc_priv),
	.init		= bootmethod_init,
	.pre_probe	= bootmethod_pre_probe,
};
