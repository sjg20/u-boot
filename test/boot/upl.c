// SPDX-License-Identifier: GPL-2.0+
/*
 * UPL handoff testing
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <abuf.h>
#include <mapmem.h>
#include <upl.h>
#include <dm/ofnode.h>
#include <test/test.h>
#include <test/ut.h>
#include "bootstd_common.h"

void upl_get_test_data(struct upl *upl)
{
	memset(upl, '\0', sizeof(struct upl));
	upl->addr_cells = 1;
	upl->size_cells = 1;
	upl->smbios = 0x123;
	upl->acpi = 0x456;
	upl->bootmode = BIT(UPLBM_DEFAULT) | BIT(UPLBM_S3);
	upl->fit = 0x789;
	upl->conf_offset = 0x234;
	upl->addr_width = 46;
	upl->acpi_nvs_size = 0x100;

	upl->num_images = 2;
	upl->image[0].load = 0x1;
	upl->image[0].size = 0x2;
	upl->image[0].offset = 0x3;
	upl->image[0].description = "U-Boot";
	upl->image[1].load = 0x4;
	upl->image[1].size = 0x5;
	upl->image[1].offset = 0x6;
	upl->image[1].description = "ATF";

	upl->num_mems = 2;
	upl->mem[0].num_regions = 3;
	upl->mem[0].region[0].base = 0x10;
	upl->mem[0].region[0].size = 0x20;
	upl->mem[0].region[1].base = 0x30;
	upl->mem[0].region[1].size = 0x40;
	upl->mem[0].region[2].base = 0x50;
	upl->mem[0].region[2].size = 0x60;
	upl->mem[1].num_regions = 1;
	upl->mem[1].region[0].base = 0x70;
	upl->mem[1].region[0].size = 0x80;
	upl->mem[1].hotpluggable = true;

	upl->num_memmaps = 5;
	upl->memmap[0].num_regions = 5;
	upl->memmap[0].name = "acpi";
	upl->memmap[0].usage = BIT(UPLUS_ACPI_RECLAIM);
	upl->memmap[0].region[0].base = 0x11;
	upl->memmap[0].region[0].size = 0x12;
	upl->memmap[0].region[1].base = 0x13;
	upl->memmap[0].region[1].size = 0x14;
	upl->memmap[0].region[2].base = 0x15;
	upl->memmap[0].region[2].size = 0x16;
	upl->memmap[0].region[3].base = 0x17;
	upl->memmap[0].region[3].size = 0x18;
	upl->memmap[0].region[4].base = 0x19;
	upl->memmap[0].region[4].size = 0x1a;
	upl->memmap[1].name = "u-boot";
	upl->memmap[1].num_regions = 1;
	upl->memmap[1].usage = BIT(UPLUS_BOOT_DATA);
	upl->memmap[1].region[0].base = 0x21;
	upl->memmap[1].region[0].size = 0x22;
	upl->memmap[2].name = "efi";
	upl->memmap[2].num_regions = 1;
	upl->memmap[2].usage = BIT(UPLUS_RUNTIME_CODE);
	upl->memmap[2].region[0].base = 0x23;
	upl->memmap[2].region[0].size = 0x24;
	upl->memmap[3].num_regions = 2;
	upl->memmap[3].name = "empty";
	upl->memmap[3].usage = 0;
	upl->memmap[3].region[0].base = 0x25;
	upl->memmap[3].region[0].size = 0x26;
	upl->memmap[3].region[1].base = 0x27;
	upl->memmap[3].region[1].size = 0x28;
	upl->memmap[4].name = "acpi-things";
	upl->memmap[4].num_regions = 1;
	upl->memmap[4].usage = BIT(UPLUS_RUNTIME_CODE) | BIT(UPLUS_ACPI_NVS);
	upl->memmap[4].region[0].base = 0x29;
	upl->memmap[4].region[0].base = 0x2a;

	upl->num_memres = 2;
	upl->memres[0].num_regions = 1;
	upl->memres[0].name = "mmio";
	upl->memres[0].region[0].base = 0x2b;
	upl->memres[0].region[0].size = 0x2c;
	upl->memres[1].num_regions = 2;
	upl->memres[1].name = "memory";
	upl->memres[1].region[0].base = 0x2d;
	upl->memres[1].region[0].size = 0x2e;
	upl->memres[1].region[1].base = 0x2f;
	upl->memres[1].region[1].size = 0x30;
	upl->memres[1].no_map = true;

	upl->serial.compatible = "ns16550a";
	upl->serial.clock_frequency = 1843200;
	upl->serial.current_speed = 115200;
	upl->serial.reg.base = 0xf1de0000;
	upl->serial.reg.size = 0x100;
	upl->serial.reg_io_shift = 2;
	upl->serial.reg_offset = 0x40;
	upl->serial.reg_io_width = 1;
	upl->serial.virtual_reg = 0x20000000;
	upl->serial.access_type = UPLSAT_MMIO;

	upl->graphics.reg.base = 0xd0000000;
	upl->graphics.reg.size = 0x10000000;
	upl->graphics.width = 1280;
	upl->graphics.height = 1280;
	upl->graphics.stride = upl->graphics.width * 4;
	upl->graphics.format = UPLGF_ARGB32;
}

static int compare_upl_image(struct unit_test_state *uts,
			     struct upl_image *base, struct upl_image *cmp)
{
	ut_asserteq(base->load, cmp->load);
	ut_asserteq(base->size, cmp->size);
	ut_asserteq(base->offset, cmp->offset);
	ut_asserteq_str(base->description, cmp->description);

	return 0;
}

static int compare_upl_memregion(struct unit_test_state *uts,
			      struct memregion *base, struct memregion *cmp)
{
	ut_asserteq(base->base, cmp->base);
	ut_asserteq(base->size, cmp->size);

	return 0;
}

static int compare_upl_mem(struct unit_test_state *uts, struct upl_mem *base,
			   struct upl_mem *cmp)
{
	int i;

	ut_asserteq(base->num_regions, cmp->num_regions);
	ut_asserteq(base->hotpluggable, cmp->hotpluggable);
	for (i = 0; i < base->num_regions; i++)
		ut_assertok(compare_upl_memregion(uts, &base->region[i],
						  &cmp->region[i]));

	return 0;
}

static int check_device_name(struct unit_test_state *uts, const char *base,
			     const char *cmp)
{
	const char *p;

	p = strchr(cmp, '@');
	if (p) {
		ut_assertnonnull(p);
		ut_asserteq_strn(base, cmp);
		ut_asserteq(p - cmp, strlen(base));
	} else {
		ut_asserteq_str(base, cmp);
	}

	return 0;
}

static int compare_upl_memmap(struct unit_test_state *uts,
			      struct upl_memmap *base, struct upl_memmap *cmp)
{
	int i;

	ut_assertok(check_device_name(uts, base->name, cmp->name));
	ut_asserteq(base->num_regions, cmp->num_regions);
	ut_asserteq(base->usage, cmp->usage);
	for (i = 0; i < base->num_regions; i++)
		ut_assertok(compare_upl_memregion(uts, &base->region[i],
						  &cmp->region[i]));

	return 0;
}

static int compare_upl_memres(struct unit_test_state *uts,
			      struct upl_memres *base, struct upl_memres *cmp)
{
	int i;

	ut_assertok(check_device_name(uts, base->name, cmp->name));
	ut_asserteq(base->num_regions, cmp->num_regions);
	ut_asserteq(base->no_map, cmp->no_map);
	for (i = 0; i < base->num_regions; i++)
		ut_assertok(compare_upl_memregion(uts, &base->region[i],
						  &cmp->region[i]));

	return 0;
}

static int compare_upl_serial(struct unit_test_state *uts,
			      struct upl_serial *base, struct upl_serial *cmp)
{
	ut_asserteq_str(base->compatible, cmp->compatible);
	ut_asserteq(base->clock_frequency, cmp->clock_frequency);
	ut_asserteq(base->current_speed, cmp->current_speed);
	ut_assertok(compare_upl_memregion(uts, &base->reg, &cmp->reg));
	ut_asserteq(base->reg_io_shift, cmp->reg_io_shift);
	ut_asserteq(base->reg_offset, cmp->reg_offset);
	ut_asserteq(base->reg_io_width, cmp->reg_io_width);
	ut_asserteq(base->virtual_reg, cmp->virtual_reg);
	ut_asserteq(base->access_type, cmp->access_type);

	return 0;
}

static int compare_upl_graphics(struct unit_test_state *uts,
				struct upl_graphics *base,
				struct upl_graphics *cmp)
{
	ut_assertok(compare_upl_memregion(uts, &base->reg, &cmp->reg));
	ut_asserteq(base->width, cmp->width);
	ut_asserteq(base->height, cmp->height);
	ut_asserteq(base->stride, cmp->stride);
	ut_asserteq(base->format, cmp->format);

	return 0;
}

static int compare_upl(struct unit_test_state *uts, struct upl *base,
		       struct upl *cmp)
{
	int i;

	ut_asserteq(base->addr_cells, cmp->addr_cells);
	ut_asserteq(base->size_cells, cmp->size_cells);

	ut_asserteq(base->smbios, cmp->smbios);
	ut_asserteq(base->acpi, cmp->acpi);
	ut_asserteq(base->bootmode, cmp->bootmode);
	ut_asserteq(base->fit, cmp->fit);
	ut_asserteq(base->conf_offset, cmp->conf_offset);
	ut_asserteq(base->addr_width, cmp->addr_width);
	ut_asserteq(base->acpi_nvs_size, cmp->acpi_nvs_size);

	ut_asserteq(base->num_images, cmp->num_images);
	for (i = 0; i < base->num_images; i++)
		ut_assertok(compare_upl_image(uts, &base->image[i],
					      &cmp->image[i]));

	ut_asserteq(base->num_mems, cmp->num_mems);
	for (i = 0; i < base->num_mems; i++)
		ut_assertok(compare_upl_mem(uts, &base->mem[i], &cmp->mem[i]));

	ut_asserteq(base->num_memmaps, cmp->num_memmaps);
	for (i = 0; i < base->num_memmaps; i++)
		ut_assertok(compare_upl_memmap(uts, &base->memmap[i],
					       &cmp->memmap[i]));

	ut_asserteq(base->num_memres, cmp->num_memres);
	for (i = 0; i < base->num_memres; i++)
		ut_assertok(compare_upl_memres(uts, &base->memres[i],
					       &cmp->memres[i]));

	ut_assertok(compare_upl_serial(uts, &base->serial, &cmp->serial));
	ut_assertok(compare_upl_graphics(uts, &base->graphics, &cmp->graphics));

	return 0;
}

/* Basic test of writing and reading UPL handoff */
static int upl_test_base(struct unit_test_state *uts)
{
	oftree tree, check_tree;
	struct upl upl, check;
	struct abuf buf;

	upl_get_test_data(&upl);

	ut_assertok(upl_create_handoff_tree(&upl, &tree));
	ut_assertok(oftree_to_fdt(tree, &buf));

	/*
	 * strings in check_tree and therefore check are only valid so long as
	 * buf stays around. As soon as we call abuf_uninit they go away
	 */
	check_tree = oftree_from_fdt(abuf_data(&buf));
	ut_assertok(ofnode_valid(oftree_path(check_tree, "/")));

	ut_assertok(upl_read_handoff(&check, check_tree));
	ut_assertok(compare_upl(uts, &upl, &check));
	abuf_uninit(&buf);

	return 0;
}
BOOTSTD_TEST(upl_test_base, 0);

/* Write an invalid structure */
static int upl_test_write_failure(struct unit_test_state *uts)
{
	struct upl upl;
	oftree tree;

	upl_get_test_data(&upl);
	upl.num_images = UPL_MAX_IMAGES;
	ut_asserteq(-E2BIG, upl_create_handoff_tree(&upl, &tree));

	upl_get_test_data(&upl);
	upl.num_mems = UPL_MAX_MEMS;
	ut_asserteq(-E2BIG, upl_create_handoff_tree(&upl, &tree));

	upl_get_test_data(&upl);
	upl.num_memmaps = UPL_MAX_MEMMAPS;
	ut_asserteq(-E2BIG, upl_create_handoff_tree(&upl, &tree));

	upl_get_test_data(&upl);
	upl.num_memres = UPL_MAX_MEMRESERVED;
	ut_asserteq(-E2BIG, upl_create_handoff_tree(&upl, &tree));

	return 0;
}
BOOTSTD_TEST(upl_test_write_failure, 0);

/**
 * add_more_nodes() - Add enough extra nodes of a given type to check failures
 *
 * Adding too many nodes of node like memory@ and memory-map subnode eventually
 * hits the limit, e,g. UPL_MAX_MEMS for /memory@ nodes. This makes more and
 * more copies of an existing node until it reaches the limit, at which point it
 * checks that upl_read_handoff() returns -E2BIG
 *
 * @uts: Unit-test state
 * @tree: Devicetree to test with
 * @dst: Parent path (e.g. "/memory-map"), or, for root nodes, a node template
 * (e.g. "memory@"). This is used to figure out what nodes to copy. In the
 * former case it copies one of the subnodes (e.g. "/memory-map/acpi@123") and
 * in the latter case it copies one of the root subnodes (e.g. /memory@123)
 * @total_nodes: Total number of nodes which upl_read_handoff() can handle
 * before failing
 * Returns 0 if OK, 1 on failure
 */
static int add_more_nodes(struct unit_test_state *uts, oftree tree,
			  const char *dst, int total_nodes)
{
	ofnode parent, to_copy, node;
	bool top_level;
	struct upl check;
	int count;

	/* Check we should create subnodes in the root node (at top level)  */
	top_level = *dst != '/';

	parent = top_level ? oftree_root(tree) : oftree_path(tree, dst);
	ut_assert(ofnode_valid(parent));

	/* count the number of nodes in the parent and find a node to copy */
	count = 0;
	ofnode_for_each_subnode(node, parent) {
		if (top_level &&
		    strncmp(dst, ofnode_get_name(node), strlen(dst)))
			continue;
		to_copy = node;
		count++;
	}

	/* add one node at a time until it fails */
	for (; count < total_nodes;) {
		char str[30];

		if (top_level)
			sprintf(str, "%s%d", dst, count + 1);
		else
			sprintf(str, "any-%d", count + 1);
		ut_assertok(ofnode_copy_node(parent, str, to_copy, &node));
		if (++count < total_nodes)
			ut_assertok(upl_read_handoff(&check, tree));
		else
			ut_asserteq(-E2BIG, upl_read_handoff(&check, tree));
	}

	/* delete the last node and see that it works */
	ut_assertok(ofnode_delete(&node));
	ut_assertok(upl_read_handoff(&check, tree));

	return 0;
}

/* Read a structure we cannot parse */
static int upl_test_read_failure(struct unit_test_state *uts)
{
	oftree base_tree, tree;
	struct abuf buf;
	struct upl upl;

	upl_get_test_data(&upl);
	ut_assertok(upl_create_handoff_tree(&upl, &base_tree));
	ut_assertok(oftree_to_fdt(base_tree, &buf));
	tree = oftree_from_fdt(abuf_data(&buf));

	/* Add more and more nodes to /options/upl-image until it fails */
	ut_assertok(add_more_nodes(uts, tree, UPLPATH_UPL_IMAGE,
				   UPL_MAX_IMAGES + 1));

	/*
	 * Do the same with the other node which have implementation-defined
	 * limits
	 */
	ut_assertok(add_more_nodes(uts, tree, UPLN_MEMORY "@",
				   UPL_MAX_MEMS + 1));
	ut_assertok(add_more_nodes(uts, tree, UPLPATH_MEMORY_MAP,
				   UPL_MAX_MEMMAPS + 1));
	ut_assertok(add_more_nodes(uts, tree, UPLPATH_MEMORY_RESERVED,
				   UPL_MAX_MEMRESERVED + 1));

	return 0;
}
BOOTSTD_TEST(upl_test_read_failure, 0);
