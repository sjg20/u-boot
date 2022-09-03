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
	ofnode parent, dest_parent, root, node;
	oftree fit;

	/* Get the image node with requests in it */
	log_debug("fit=%p, noffset=%d\n", images->fit_hdr_os,
		  images->fit_noffset_os);
	fit = oftree_from_fdt(images->fit_hdr_os);
	root = oftree_root(fit);
	if (of_live_active()) {
		log_warning("Cannot fix up live tree\n");
		return 0;
	}
	if (!ofnode_valid(root))
		return log_msg_ret("rt", -EINVAL);
	parent = noffset_to_ofnode(root, images->fit_noffset_os);
	if (!ofnode_valid(parent))
		return log_msg_ret("img", -EINVAL);
	dest_parent = oftree_path(fixup->tree, "/chosen");
	if (!ofnode_valid(dest_parent))
		return log_msg_ret("dst", -EINVAL);

	node = ofnode_first_subnode(parent);
	ofnode_for_each_subnode(node, parent) {
		const char *name = ofnode_get_name(node);
		ofnode dest;
		int ret;

		log_info("processing node: %s\n", name);
		ret = ofnode_add_subnode(dest_parent, name, &dest);
		if (ret && ret != -EEXIST)
			return log_msg_ret("add", ret);
		ret = ofnode_copy_props(node, dest);
		if (ret)
			return log_msg_ret("cp", ret);
	}

	return 0;
}
EVENT_SPY(EVT_FT_FIXUP, bootmeth_vbe_ft_fixup);
