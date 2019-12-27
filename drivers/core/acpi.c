// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define DEBUG

#include <common.h>
#include <acpi.h>
#include <dm.h>
#include <asm/acpigen.h>
#include <dm/root.h>

#define MAX_ITEMS	100

enum gen_type_t {
	TYPE_DSDT,
	TYPE_SSDT,
};

struct acpi_item {
	struct udevice *dev;
	enum gen_type_t type;
	char *buf;
	int size;
};

static struct acpi_item acpi_item[MAX_ITEMS];
static int item_count;

static char *ordering[] = {
	"board",
	"cpu@0",
	"i2c2@16,0",
	"i2c2@16,1",
	"i2c2@16,2",
	"i2c2@16,3",
	"i2c2@17,0",
	"i2c2@17,1",
	"sdmmc@1b,0",
	"maxim-codec",
	"wifi",
	"da-codec",
	"pci_mmc",
	NULL,
};

static int acpi_add_item(struct udevice *dev, enum gen_type_t type, void *start)
{
	struct acpi_item *item;
	void *end = acpigen_get_current();

	if (item_count == MAX_ITEMS) {
		printf("Too many items\n");
		return log_msg_ret("mem", -ENOSPC);
	}

	item = &acpi_item[item_count];
	item->dev = dev;
	item->type = type;
	item->size = end - start;
	if (!item->size)
		return 0;
	item->buf = malloc(item->size);
	if (!item->buf)
		return log_msg_ret("mem", -ENOMEM);
	memcpy(item->buf, start, item->size);
	item_count++;
	printf("* %s: Added type %d, %p, size %x\n", dev->name, type, start,
	       item->size);

	return 0;
}

void acpi_dump_items(void)
{
	int i;

	for (i = 0; i < item_count; i++) {
		struct acpi_item *item = &acpi_item[i];

		printf("dev '%s', type %d, size %x\n", item->dev->name,
		       item->type, item->size);
// 		print_buffer(0, item->buf, 1, item->size, 0);
// 		printf("\n");
	}
}

struct acpi_item *find_item(const char *devname)
{
	int i;

	for (i = 0; i < item_count; i++) {
		struct acpi_item *item = &acpi_item[i];

		if (!strcmp(devname, item->dev->name))
			return item;
	}

	return NULL;
}

static int build_type(void *start, enum gen_type_t type)
{
	void *ptr;
	void *end = acpigen_get_current();
	char **strp;

	ptr = start;
	for (strp = ordering; *strp; strp++) {
		struct acpi_item *item;

		item = find_item(*strp);
		if (!item) {
			printf("Failed to file item '%s'\n", *strp);
		} else if (item->type == type) {
			printf("   - add %s\n", item->dev->name);
			memcpy(ptr, item->buf, item->size);
			ptr += item->size;
		}
	}

	if (ptr != end) {
		printf("*** Missing bytes: ptr=%p, end=%p\n", ptr, end);
		return -ENXIO;
	}

	return 0;
}

int acpi_return_name(char *out_name, const char *name)
{
	strcpy(out_name, name);

	return 0;
}

int acpi_align(struct acpi_ctx *ctx)
{
	ctx->current = ALIGN(ctx->current, 16);

	return 0;
}

int _acpi_dev_write_tables(struct udevice *parent, struct acpi_ctx *ctx)
{
	struct acpi_ops *aops;
	struct udevice *dev;
	int ret;

	aops = device_get_acpi_ops(parent);
	if (aops && aops->write_tables) {
		debug("- %s\n", parent->name);
		ret = aops->write_tables(parent, ctx);
		if (ret)
			return ret;
	}
	device_foreach_child(dev, parent) {
		ret = _acpi_dev_write_tables(dev, ctx);
		if (ret)
			return ret;
	}

	return 0;
}

int acpi_dev_write_tables(struct acpi_ctx *ctx)
{
	int ret;

	debug("Writing device tables\n");
	ret = _acpi_dev_write_tables(dm_root(), ctx);
	debug("Writing finished, err=%d\n", ret);

	return ret;
}

int _acpi_fill_ssdt_generator(struct udevice *parent, struct acpi_ctx *ctx)
{
	struct acpi_ops *aops;
	struct udevice *dev;
	int ret;

	aops = device_get_acpi_ops(parent);
	if (aops && aops->fill_ssdt_generator) {
		void *start = acpigen_get_current();

		debug("- %s %p\n", parent->name, aops->fill_ssdt_generator);
		ret = aops->fill_ssdt_generator(parent, ctx);
		if (ret)
			return ret;
		ret = acpi_add_item(parent, TYPE_SSDT, start);
		if (ret)
			return ret;
	}
	device_foreach_child(dev, parent) {
		ret = _acpi_fill_ssdt_generator(dev, ctx);
		if (ret)
			return ret;
	}

	return 0;
}

int acpi_fill_ssdt_generator(struct acpi_ctx *ctx)
{
	void *start = acpigen_get_current();
	int ret;

	debug("Writing SSDT tables\n");
	ret = _acpi_fill_ssdt_generator(dm_root(), ctx);
	debug("Writing SSDT finished, err=%d\n", ret);
	build_type(start, TYPE_SSDT);

	return ret;
}

int _acpi_inject_dsdt_generator(struct udevice *parent, struct acpi_ctx *ctx)
{
	struct acpi_ops *aops;
	struct udevice *dev;
	int ret;

	aops = device_get_acpi_ops(parent);
	if (aops && aops->inject_dsdt_generator) {
		void *start = acpigen_get_current();

		debug("- %s %p\n", parent->name, aops->inject_dsdt_generator);
		ret = aops->inject_dsdt_generator(parent, ctx);
		if (ret)
			return ret;
		ret = acpi_add_item(parent, TYPE_DSDT, start);
		if (ret)
			return ret;
	}
	device_foreach_child(dev, parent) {
		ret = _acpi_inject_dsdt_generator(dev, ctx);
		if (ret)
			return ret;
	}

	return 0;
}

int acpi_inject_dsdt_generator(struct acpi_ctx *ctx)
{
	void *start = acpigen_get_current();
	int ret;

	debug("Writing DSDT tables\n");
	ret = _acpi_inject_dsdt_generator(dm_root(), ctx);
	debug("Writing DSDT finished, err=%d\n", ret);
	build_type(start, TYPE_DSDT);

	return ret;
}

int acpi_dp_add_integer_from_dt(struct udevice *dev, struct acpi_dp *dp,
				const char *prop)
{
	int ret;
	u32 val = 0;

	ret = dev_read_u32(dev, prop, &val);
	acpi_dp_add_integer(dp, prop, val);

	return ret;
}

int acpi_dp_add_string_from_dt(struct udevice *dev, struct acpi_dp *dp,
			       const char *prop)
{
	const char *val;

	val = dev_read_string(dev, prop);
	if (!val)
		return -ENOENT;
	acpi_dp_add_string(dp, prop, val);

	return 0;
}
