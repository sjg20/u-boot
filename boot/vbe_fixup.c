// SPDX-License-Identifier: GPL-2.0
/*
 * Verified Boot for Embedded (VBE) device tree fixup functions
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_BOOT

#include <common.h>
#include <event.h>
#include <image.h>
#include <dm/ofnode.h>

/**
 * bootmeth_vbe_ft_fixup() - Process VBE OS requests and do device tree fixups
 *
 * @ctx: Context for event
 * @event: Event to process
 * @return 0 if OK, -ve on error
 */
static int bootmeth_vbe_ft_fixup(void *ctx, struct event *event)
{
	const struct event_ft_fixup *fixup = &event->data.ft_fixup;
	const struct bootm_headers *images = fixup->images;
	oftree tree = fixup->tree;
	ofnode parent, root, node;
	oftree fit;

	/* Get the image node with requests in it */
	log_info("fit=%p, noffset=%d\n", images->fit_hdr_os,
		 images->fit_noffset_os);
	fit = oftree_from_fdt(images->fit_hdr_os);
	root = ofnode_path_root(tree, "/");
	if (of_live_active()) {
		log_warning("Cannot fix up live tree\n");
		return 0;
	}
	printf("here\n");
	if (!ofnode_valid(root))
		return log_msg_ret("rt", -EINVAL);
	parent = noffset_to_ofnode(root, images->fit_noffset_os);
	if (!ofnode_valid(parent))
		return log_msg_ret("img", -EINVAL);

	ofnode_for_each_subnode(node, parent) {
		log_info("processing node: %s\n", ofnode_get_name(node));
	}

	return 0;
}
EVENT_SPY(EVT_FT_FIXUP, bootmeth_vbe_ft_fixup);
