// SPDX-License-Identifier: GPL-2.0+
/*
 * UPL handoff generation
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <common.h>
#include <log.h>
#include <upl.h>
#include <dm/ofnode.h>
#include "upl_common.h"

/**
 * write_addr() - Write an address
 *
 * Writes an address in the correct format, either 32- or 64-bit
 *
 * @upl: UPL state
 * @node: Node to write to
 * @prop: Property name to write
 * @addr: Address to write
 * Return: 0 if OK, -ve on error
 */
static int write_addr(const struct upl *upl, ofnode node, const char *prop,
		      ulong addr)
{
	int ret;

	if (upl->addr_cells == 1)
		ret = ofnode_write_u32(node, prop, addr);
	else
		ret = ofnode_write_u64(node, prop, addr);

	return ret;
}

/**
 * write_size() - Write a size
 *
 * Writes a size in the correct format, either 32- or 64-bit
 *
 * @upl: UPL state
 * @node: Node to write to
 * @prop: Property name to write
 * @size: Size to write
 * Return: 0 if OK, -ve on error
 */
static int write_size(const struct upl *upl, ofnode node, const char *prop,
		      ulong size)
{
	int ret;

	if (upl->size_cells == 1)
		ret = ofnode_write_u32(node, prop, size);
	else
		ret = ofnode_write_u64(node, prop, size);

	return ret;
}

/**
 * ofnode_write_bitmask() - Write a bit mask as a string list
 *
 * @node: Node to write to
 * @prop: Property name to write
 * @names: Array of names for each bit
 * @count: Number of array entries
 * @value: Bit-mask value to write
 * Return: 0 if OK, -EINVAL if a bit number is not defined, -ENOSPC if the
 * string is too long for the (internal) buffer
 */
static int ofnode_write_bitmask(ofnode node, const char *prop,
				const char *const names[], uint count,
				uint value)
{
	char buf[128];
	char *ptr, *end = buf + sizeof(buf);
	uint bit;
	int ret;

	ptr = buf;
	for (bit = 0; bit < count; bit++) {
		if (value & BIT(bit)) {
			const char *str = names[bit];
			uint len;

			if (!str) {
				log_debug("Unnamed bit number %d\n", bit);
				return log_msg_ret("bit", -EINVAL);
			}
			len = strlen(str) + 1;
			if (ptr + len > end) {
				log_debug("String array too long\n");
				return log_msg_ret("bit", -ENOSPC);
			}

			memcpy(ptr, str, len);
			ptr += len;
		}
	}

	ret = ofnode_write_prop(node, prop, buf, ptr - buf, true);
	if (ret)
		return log_msg_ret("wri", ret);

	return 0;
}

/**
 * ofnode_write_value() - Write an int as a string value using a lookup
 *
 * @node: Node to write to
 * @prop: Property name to write
 * @names: Array of names for each int value
 * @count: Number of array entries
 * @value: Int value to write
 * Return: 0 if OK, -EINVAL if a bit number is not defined, -ENOSPC if the
 * string is too long for the (internal) buffer
 */
static int ofnode_write_value(ofnode node, const char *prop,
			      const char *const names[], uint count,
			      uint value)
{
	const char *str;
	int ret;

	if (value >= count) {
		log_debug("Value of of range %d\n", value);
		return log_msg_ret("val", -ERANGE);
	}
	str = names[value];
	if (!str) {
		log_debug("Unnamed value %d\n", value);
		return log_msg_ret("val", -EINVAL);
	}
	ret = ofnode_write_string(node, prop, str);
	if (ret)
		return log_msg_ret("wri", ret);

	return 0;
}

/**
 * add_root_props() - Add root properties to the tree
 *
 * @node: Node to add to
 * Return 0 if OK, -ve on error
 */
static int add_root_props(const struct upl *upl, ofnode node)
{
	int ret;

	ret = ofnode_write_u32(node, UPLP_ADDRESS_CELLS, upl->addr_cells);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_SIZE_CELLS, upl->size_cells);
	if (ret)
		return log_msg_ret("cel", ret);

	return 0;
}

/**
 * add_upl_params() - Add UPL parameters node
 *
 * @upl: UPL state
 * @options: /options node to add to
 * Return 0 if OK, -ve on error
 */
static int add_upl_params(const struct upl *upl, ofnode options)
{
	ofnode node;
	int ret;

	ret = ofnode_add_subnode(options, UPLN_UPL_PARAMS, &node);
	if (ret)
		return log_msg_ret("img", ret);

	ret = write_addr(upl, node, UPLP_SMBIOS, upl->smbios);
	if (!ret)
		ret = write_addr(upl, node, UPLP_ACPI, upl->acpi);
	if (!ret && upl->bootmode)
		ret = ofnode_write_bitmask(node, UPLP_BOOTMODE, bootmode_names,
					   UPLBM_COUNT, upl->bootmode);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_ADDR_WIDTH, upl->addr_width);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_ACPI_NVS_SIZE,
				       upl->acpi_nvs_size);
	if (ret)
		return log_msg_ret("cnf", ret);

	return 0;
}

/**
 * add_upl_image() - Add /options/upl-image nodes and properties to the tree
 *
 * @upl: UPL state
 * @node: /options node to add to
 * Return 0 if OK, -ve on error
 */
static int add_upl_image(const struct upl *upl, ofnode options)
{
	ofnode node;
	int ret, i;

	ret = ofnode_add_subnode(options, UPLN_UPL_IMAGE, &node);
	if (ret)
		return log_msg_ret("img", ret);

	if (upl->fit)
		ret = ofnode_write_u32(node, UPLP_FIT, upl->fit);
	if (!ret && upl->conf_offset)
		ret = ofnode_write_u32(node, UPLP_CONF_OFFSET,
				       upl->conf_offset);
	if (ret)
		return log_msg_ret("cnf", ret);

	if (upl->num_images >= UPL_MAX_IMAGES) {
		log_debug("Exceeded image limit %d\n", UPL_MAX_IMAGES);
		return log_msg_ret("img", -E2BIG);
	}
	for (i = 0; i < upl->num_images; i++) {
		const struct upl_image *img = &upl->image[i];
		char name[10];
		ofnode subnode;

		snprintf(name, sizeof(name), UPLN_IMAGE "-%d", i + 1);
		ret = ofnode_add_subnode(node, name, &subnode);
		if (ret)
			return log_msg_ret("sub", ret);

		ret = write_addr(upl, subnode, UPLP_LOAD, img->load);
		if (!ret)
			ret = write_size(upl, subnode, UPLP_SIZE, img->size);
		if (!ret && img->offset)
			ret = ofnode_write_u32(subnode, UPLP_OFFSET,
					       img->offset);
		ret = ofnode_write_string(subnode, UPLP_DESCRIPTION,
					  img->description);
		if (ret)
			return log_msg_ret("sim", ret);
	}

	return 0;
}

/**
 * buffer_addr_size() - Generate a set of addr/size pairs
 *
 * Each base/size value from each region is written to the buffer in a suitable
 * format to be written to the devicetree
 *
 * @upl: UPL state
 * @buf: Buffer to write to
 * @size: Buffer size
 * @num_regions: Number of regions to process
 * @regions: List of regions to process
 * Returns: Number of bytes written, or -ENOSPC if the buffer is too small
 */
static int buffer_addr_size(const struct upl *upl, char *buf, int size,
			    uint num_regions, const struct memregion *regions)
{
	char *ptr, *end = buf + size;
	int i;

	ptr = buf;
	for (i = 0; i < num_regions; i++) {
		const struct memregion *reg = &regions[i];

		if (upl->addr_cells == 1)
			*(u32 *)ptr = cpu_to_fdt32(reg->base);
		else
			*(u64 *)ptr = cpu_to_fdt64(reg->base);
		ptr += upl->addr_cells * sizeof(u32);

		if (upl->size_cells == 1)
			*(u32 *)ptr = cpu_to_fdt32(reg->size);
		else
			*(u64 *)ptr = cpu_to_fdt64(reg->size);
		ptr += upl->size_cells * sizeof(u32);
		if (ptr > end)
			return -ENOSPC;
	}

	return ptr - buf;
}

/**
 * add_upl_memory() - Add /memory nodes to the tree
 *
 * @upl: UPL state
 * @root: Parent node to contain the new /memory nodes
 * Return 0 if OK, -ve on error
 */
static int add_upl_memory(const struct upl *upl, ofnode root)
{
	int i;

	if (upl->num_mems >= UPL_MAX_MEMS) {
		log_debug("Exceeded mems limit %d\n", UPL_MAX_IMAGES);
		return log_msg_ret("img", -E2BIG);
	}

	for (i = 0; i < upl->num_mems; i++) {
		const struct upl_mem *mem = &upl->mem[i];
		char buf[UPL_MAX_MEMREGIONS * sizeof(64) * 2];
		char name[26];
		int ret, len;
		ofnode node;

		if (!mem->num_regions) {
			log_debug("Memory %d has no regions\n", i);
			return log_msg_ret("reg", -EINVAL);
		}
		sprintf(name, UPLN_MEMORY "@0x%lx", mem->region[0].base);
		ret = ofnode_add_subnode(root, name, &node);
		if (ret)
			return log_msg_ret("mem", ret);

		len = buffer_addr_size(upl, buf, sizeof(buf), mem->num_regions,
				       mem->region);
		if (len < 0)
			return log_msg_ret("buf", len);

		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
		if (!ret && mem->hotpluggable)
			ret = ofnode_write_bool(node, UPLP_HOTPLUGGABLE,
						mem->hotpluggable);
		if (ret)
			return log_msg_ret("lst", ret);
	}

	return 0;
}

/**
 * add_upl_memmap() - Add memory-map nodes to the tree
 *
 * @upl: UPL state
 * @root: Parent node to contain the new /memory-map node and its subnodes
 * Return 0 if OK, -ve on error
 */
static int add_upl_memmap(const struct upl *upl, ofnode root)
{
	ofnode mem_node;
	int i, ret;

	if (!upl->num_memmaps)
		return 0;
	ret = ofnode_add_subnode(root, UPLN_MEMORY_MAP, &mem_node);
	if (ret)
		return log_msg_ret("img", ret);

	if (upl->num_memmaps >= UPL_MAX_MEMMAPS) {
		log_debug("Exceeded memmap limit %d\n", UPL_MAX_IMAGES);
		return log_msg_ret("img", -E2BIG);
	}

	for (i = 0; i < upl->num_memmaps; i++) {
		const struct upl_memmap *memmap = &upl->memmap[i];
		char buf[UPL_MAX_MEMREGIONS * sizeof(64) * 2];
		char name[26];
		int ret, len;
		ofnode node;

		if (!memmap->num_regions) {
			log_debug("Memory %d has no regions\n", i);
			return log_msg_ret("reg", -EINVAL);
		}
		sprintf(name, "%s@0x%lx", memmap->name, memmap->region[0].base);
		ret = ofnode_add_subnode(mem_node, name, &node);
		if (ret)
			return log_msg_ret("memmap", ret);

		len = buffer_addr_size(upl, buf, sizeof(buf),
				       memmap->num_regions, memmap->region);
		if (len < 0)
			return log_msg_ret("buf", len);
		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
		if (!ret && memmap->usage)
			ret = ofnode_write_bitmask(node, UPLP_USAGE,
						   usage_names,
						   UPLUS_COUNT, memmap->usage);
		if (ret)
			return log_msg_ret("lst", ret);
	}

	return 0;
}

/**
 * add_upl_memres() - Add /memory-reserved nodes to the tree
 *
 * @upl: UPL state
 * @root: Parent node to contain the new node
 * Return 0 if OK, -ve on error
 */
static int add_upl_memres(const struct upl *upl, ofnode root,
			  bool skip_existing)
{
	ofnode mem_node;
	int i, ret;

	if (!upl->num_memmaps)
		return 0;
	ret = ofnode_add_subnode(root, UPLN_MEMORY_RESERVED, &mem_node);
	if (ret) {
		if (skip_existing && ret == -EEXIST)
			return 0;
		return log_msg_ret("img", ret);
	}

	if (upl->num_memres >= UPL_MAX_MEMRESERVED) {
		log_debug("Exceeded memres limit %d\n", UPL_MAX_IMAGES);
		return log_msg_ret("img", -E2BIG);
	}

	for (i = 0; i < upl->num_memres; i++) {
		const struct upl_memres *memres = &upl->memres[i];
		char buf[UPL_MAX_MEMREGIONS * sizeof(64) * 2];
		char name[26];
		int ret, len;
		ofnode node;

		if (!memres->num_regions) {
			log_debug("Memory %d has no regions\n", i);
			return log_msg_ret("reg", -EINVAL);
		}
		sprintf(name, "%s@0x%lx", memres->name, memres->region[0].base);
		ret = ofnode_add_subnode(mem_node, name, &node);
		if (ret)
			return log_msg_ret("memres", ret);

		len = buffer_addr_size(upl, buf, sizeof(buf),
				       memres->num_regions, memres->region);
		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
		if (!ret && memres->no_map)
			ret = ofnode_write_bool(node, UPLP_NO_MAP,
						memres->no_map);
		if (ret)
			return log_msg_ret("lst", ret);
	}

	return 0;
}

/**
 * add_upl_serial() - Add serial node
 *
 * @upl: UPL state
 * @root: Parent node to contain the new node
 * Return 0 if OK, -ve on error
 */
static int add_upl_serial(const struct upl *upl, ofnode root,
			  bool skip_existing)
{
	const struct upl_serial *ser = &upl->serial;
	char name[26];
	ofnode node;
	int ret;

	if (!ser->compatible || skip_existing)
		return 0;
	sprintf(name, UPLN_SERIAL "@0x%lx", ser->reg.base);
	ret = ofnode_add_subnode(root, name, &node);
	if (ret)
		return log_msg_ret("img", ret);
	ret = ofnode_write_string(node, UPLP_COMPATIBLE, ser->compatible);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_CLOCK_FREQUENCY,
				       ser->clock_frequency);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_CURRENT_SPEED,
				       ser->current_speed);
	if (!ret) {
		char buf[16];
		int len;

		len = buffer_addr_size(upl, buf, sizeof(buf), 1, &ser->reg);
		if (len < 0)
			return log_msg_ret("buf", len);

		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
	}
	if (!ret && ser->reg_io_shift != UPLD_REG_IO_SHIFT)
		ret = ofnode_write_u32(node, UPLP_REG_IO_SHIFT,
				       ser->reg_io_shift);
	if (!ret && ser->reg_offset != UPLD_REG_OFFSET)
		ret = ofnode_write_u32(node, UPLP_REG_OFFSET, ser->reg_offset);
	if (!ret && ser->reg_io_width != UPLD_REG_IO_WIDTH)
		ret = ofnode_write_u32(node, UPLP_REG_IO_WIDTH,
				       ser->reg_io_width);
	if (!ret && ser->virtual_reg)
		ret = write_addr(upl, node, UPLP_VIRTUAL_REG, ser->virtual_reg);
	if (!ret) {
		ret = ofnode_write_value(node, UPLP_ACCESS_TYPE, access_types,
					 ARRAY_SIZE(access_types),
					 ser->access_type);
	}
	if (ret)
		return log_msg_ret("ser", ret);

	return 0;
}

/**
 * add_upl_graphics() - Add graphics node
 *
 * @upl: UPL state
 * @root: Parent node to contain the new node
 * Return 0 if OK, -ve on error
 */
static int add_upl_graphics(const struct upl *upl, ofnode root)
{
	const struct upl_graphics *gra = &upl->graphics;
	char name[36];
	ofnode node;
	int ret;

	sprintf(name, UPLN_GRAPHICS "@0x%lx", gra->reg.base);
	ret = ofnode_add_subnode(root, name, &node);
	if (ret)
		return log_msg_ret("gra", ret);

	ret = ofnode_write_string(node, UPLP_COMPATIBLE, UPLC_GRAPHICS);
	if (!ret) {
		char buf[16];
		int len;

		len = buffer_addr_size(upl, buf, sizeof(buf), 1, &gra->reg);
		if (len < 0)
			return log_msg_ret("buf", len);

		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
	}
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_WIDTH, gra->width);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_HEIGHT, gra->height);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_STRIDE, gra->stride);
	if (!ret) {
		ret = ofnode_write_value(node, UPLP_GRAPHICS_FORMAT,
					 graphics_formats,
					 ARRAY_SIZE(graphics_formats),
					 gra->format);
	}
	if (ret)
		return log_msg_ret("pro", ret);

	return 0;
}

int upl_write_handoff(const struct upl *upl, ofnode root, bool skip_existing)
{
	ofnode options;
	int ret;

	ret = add_root_props(upl, root);
	if (ret)
		return log_msg_ret("ad1", ret);
	ret = ofnode_add_subnode(root, UPLN_OPTIONS, &options);
	if (ret && ret != -EEXIST)
		return log_msg_ret("opt", -EINVAL);

	ret = add_upl_params(upl, options);
	if (ret)
		return log_msg_ret("ad1", ret);

	ret = add_upl_image(upl, options);
	if (ret)
		return log_msg_ret("ad2", ret);

	ret = add_upl_memory(upl, root);
	if (ret)
		return log_msg_ret("ad3", ret);

	ret = add_upl_memmap(upl, root);
	if (ret)
		return log_msg_ret("ad4", ret);

	ret = add_upl_memres(upl, root, skip_existing);
	if (ret)
		return log_msg_ret("ad5", ret);

	ret = add_upl_serial(upl, root, skip_existing);
	if (ret)
		return log_msg_ret("ad6", ret);

	ret = add_upl_graphics(upl, root);
	if (ret)
		return log_msg_ret("ad6", ret);

	return 0;
}

int upl_create_handoff_tree(const struct upl *upl, oftree *treep)
{
	ofnode root;
	oftree tree;
	int ret;

	ret = oftree_new(&tree);
	if (ret)
		return log_msg_ret("new", ret);

	root = oftree_root(tree);
	if (!ofnode_valid(root))
		return log_msg_ret("roo", -EINVAL);

	ret = upl_write_handoff(upl, root, false);
	if (ret)
		return log_msg_ret("wr", ret);

	*treep = tree;

	return 0;
}
