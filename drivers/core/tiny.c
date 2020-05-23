// SPDX-License-Identifier: GPL-2.0+
/*
 * Support for tiny device (those without a fully uclass and driver)
 *
 * Copyright 2020 Google LLC
 */

#define LOG_DEBUG

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>

struct tiny_dev *tiny_dev_find(enum uclass_id uclass_id, int seq)
{
	struct tiny_dev *info = ll_entry_start(struct tiny_dev, tiny_dev);
	const int n_ents = ll_entry_count(struct tiny_dev, tiny_dev);
	struct tiny_dev *entry;

	for (entry = info; entry != info + n_ents; entry++) {
		struct tiny_drv *drv = entry->drv;

		log_debug("entry %p, uclass %d %d\n", entry,
			  drv->uclass_id, uclass_id);
		if (drv->uclass_id == uclass_id) {
			if (CONFIG_IS_ENABLED(TINY_RELOC)) {
				struct tiny_dev *copy;

				copy = malloc(sizeof(*copy));
				if (!copy)
					return NULL;
				memcpy(copy, entry, sizeof(*copy));
				return copy;
			}
			return entry;
		}
	}

	return NULL;
}

int tiny_dev_probe(struct tiny_dev *tdev)
{
	struct tiny_drv *drv;
	int ret;

	if (tdev->flags & DM_FLAG_ACTIVATED)
		return 0;
	drv = tdev->drv;
// 	printf("drv->priv_size=%d, tdev->priv=%p\n", drv->priv_size, tdev->priv);

	if (!tdev->priv && drv->priv_size) {
		tdev->priv = calloc(1, drv->priv_size);
// 		printf("alloced %p %p\n", tdev, tdev->priv);
		if (!tdev->priv)
			return -ENOMEM;
	}
// 	printf("2drv->priv_size=%d, tdev->priv=%p\n", drv->priv_size, tdev->priv);
	if (drv->probe) {
		ret = drv->probe(tdev);
		if (ret)
			return log_msg_ret("probe", ret);
	}

	tdev->flags |= DM_FLAG_ACTIVATED;

	return 0;
}
