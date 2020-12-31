// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015, Bin Meng <bmeng.cn@gmail.com>
 *
 * Adapted from coreboot src/arch/x86/smbios.c
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <mapmem.h>
#include <smbios.h>
#include <sysinfo.h>
#include <tables_csum.h>
#include <version.h>
#ifdef CONFIG_CPU
#include <cpu.h>
#include <dm/uclass-internal.h>
#endif

/**
 * struct smbios_ctx - context for writing SMBIOS tables
 *
 * @node:	node containing the information to write (ofnode_null() if none)
 * @dev:	sysinfo device to use (NULL if none)
 */
struct smbios_ctx {
	ofnode node;
	struct udevice *dev;
};

/**
 * Function prototype to write a specific type of SMBIOS structure
 *
 * @addr:	start address to write the structure
 * @handle:	the structure's handle, a unique 16-bit number
 * @return:	size of the structure
 */
typedef int (*smbios_write_type)(ulong *addr, int handle,
				 struct smbios_ctx *ctx);

/**
 * struct smbios_write_method - Information about a table-writing function
 *
 * @write: Function to call
 * @subnode_name: Name of subnode which has the information for this function,
 *	NULL if none
 */
struct smbios_write_method {
	smbios_write_type write;
	const char *subnode_name;
};

/**
 * smbios_add_string() - add a string to the string area
 *
 * This adds a string to the string area which is appended directly after
 * the formatted portion of an SMBIOS structure. If the string is already
 * present in the table, it is not added again, but the string number of the
 * existing string is returned
 *
 * @start:	string area start address
 * @str:	string to add
 * @pad:	length to pad string to (excluding terminator), 0 if none
 * @strp:	If non-NULL, returns a pointer to the string in the table
 * @return:	string number in the string area (1 or more)
 */
static int smbios_add_string_pad(char *start, const char *str, uint pad,
				 char **strp)
{
	int i = 1;
	char *p = start;

	if (strp)
		*strp = NULL;
	if (!*str)
		str = "Unknown";

	for (;;) {
		if (!*p) {
			uint len;

			if (strp)
				*strp = p;
			strcpy(p, str);
			len = strlen(str);
			p += len;
			if (pad > len) {
				memset(p, ' ', pad - len);
				p += pad - len;
			}
			*p++ = '\0';
			*p++ = '\0';

			return i;
		}

		if (!strcmp(p, str)) {
			if (strp)
				*strp = p;
			return i;
		}

		p += strlen(p) + 1;
		i++;
	}
}

/**
 * smbios_add_string() - add a string to the string area
 *
 * This adds a string to the string area which is appended directly after
 * the formatted portion of an SMBIOS structure.
 *
 * @start:	string area start address
 * @str:	string to add
 * @return:	string number in the string area (1 or more)
 */
static int smbios_add_string(char *start, const char *str)
{
	return smbios_add_string_pad(start, str, 0, NULL);
}

/**
 * smbios_add_prop_si() - Add a property from the devicetree or sysinfo
 *
 * Sysinfo is used if available, with a fallback to devicetree
 *
 * @start:	string area start address
 * @node:	node containing the information to write (ofnode_null() if none)
 * @prop:	property to write
 * @return 0 if not found, else SMBIOS string number (1 or more)
 */
static int smbios_add_prop_si(char *start, struct smbios_ctx *ctx,
			      const char *prop, int sysinfo_id)
{
	if (sysinfo_id && ctx->dev) {
		char val[80];
		int ret;

		ret = sysinfo_get_str(ctx->dev, sysinfo_id, sizeof(val), val);
		if (!ret)
			return smbios_add_string(start, val);
	}
	if (IS_ENABLED(CONFIG_OF_CONTROL)) {
		const char *str;

		str = ofnode_read_string(ctx->node, prop);
		if (str)
			return smbios_add_string(start, str);
	}

	return 0;
}

/**
 * smbios_add_prop() - Add a property from the devicetree
 *
 * @start:	string area start address
 * @node:	node containing the information to write (ofnode_null() if none)
 * @prop:	property to write
 * @return 0 if not found, else SMBIOS string number (1 or more)
 */
static int smbios_add_prop(char *start, struct smbios_ctx *ctx,
			   const char *prop)
{
	return smbios_add_prop_si(start, ctx, prop, SYSINFO_ID_NONE);
}

/**
 * smbios_string_table_len() - compute the string area size
 *
 * This computes the size of the string area including the string terminator.
 *
 * @start:	string area start address
 * @return:	string area size
 */
static int smbios_string_table_len(char *start)
{
	char *p = start;
	int i, len = 0;

	while (*p) {
		i = strlen(p) + 1;
		p += i;
		len += i;
	}

	return len + 1;
}

static int smbios_write_type0(ulong *current, int handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type0 *t;
	int len = sizeof(struct smbios_type0);
	char *ver_string;
	uint pad = 0;

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type0));
	fill_smbios_header(t, SMBIOS_BIOS_INFORMATION, len, handle);
	t->vendor = smbios_add_string(t->eos, "U-Boot");

	/*
	 * Allow at least 66 bytes for version so Chrome OS can insert a new
	 * string later, of at least 64 bytes plus terminator, which is the size
	 * of chromeos_acpi_gnvs->fwid. See vboot_update_acpi().
	 */
	if (IS_ENABLED(CONFIG_CHROMEOS))
		pad = 66;
	t->bios_ver = smbios_add_string_pad(t->eos, PLAIN_VERSION, pad,
					    &ver_string);
	gd->arch.smbios_version = ver_string;
	t->bios_release_date = smbios_add_string(t->eos, U_BOOT_DMI_DATE);
#ifdef CONFIG_ROM_SIZE
	t->bios_rom_size = (CONFIG_ROM_SIZE / 65536) - 1;
#endif
	t->bios_characteristics = BIOS_CHARACTERISTICS_PCI_SUPPORTED |
				  BIOS_CHARACTERISTICS_SELECTABLE_BOOT |
				  BIOS_CHARACTERISTICS_UPGRADEABLE;
#ifdef CONFIG_GENERATE_ACPI_TABLE
	t->bios_characteristics_ext1 = BIOS_CHARACTERISTICS_EXT1_ACPI;
#endif
#ifdef CONFIG_EFI_LOADER
	t->bios_characteristics_ext1 |= BIOS_CHARACTERISTICS_EXT1_UEFI;
#endif
	t->bios_characteristics_ext2 = BIOS_CHARACTERISTICS_EXT2_TARGET;

	t->bios_major_release = U_BOOT_VERSION_NUM % 100;
	t->bios_minor_release = U_BOOT_VERSION_NUM_PATCH;
	t->ec_major_release = 0xff;
	t->ec_minor_release = 0xff;

	len = t->length + smbios_string_table_len(t->eos);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type1(ulong *current, int handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type1 *t;
	int len = sizeof(struct smbios_type1);
	char *serial_str = env_get("serial#");

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type1));
	fill_smbios_header(t, SMBIOS_SYSTEM_INFORMATION, len, handle);
	t->manufacturer = smbios_add_prop(t->eos, ctx, "manufacturer");
	t->product_name = smbios_add_prop(t->eos, ctx, "product");
	t->version = smbios_add_prop_si(t->eos, ctx, "version",
					SYSINFO_ID_SMBIOS_SYSTEM_VERSION);
	if (serial_str) {
		t->serial_number = smbios_add_string(t->eos, serial_str);
		strncpy((char *)t->uuid, serial_str, sizeof(t->uuid));
	} else {
		t->serial_number = smbios_add_prop(t->eos, ctx, "serial");
	}
	t->sku_number = smbios_add_prop(t->eos, ctx, "sku");
	t->family = smbios_add_prop(t->eos, ctx, "family");

	len = t->length + smbios_string_table_len(t->eos);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type2(ulong *current, int handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type2 *t;
	int len = sizeof(struct smbios_type2);

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type2));
	fill_smbios_header(t, SMBIOS_BOARD_INFORMATION, len, handle);
	t->manufacturer = smbios_add_prop(t->eos, ctx, "manufacturer");
	t->product_name = smbios_add_prop(t->eos, ctx, "product");
	t->version = smbios_add_prop_si(t->eos, ctx, "version",
					SYSINFO_ID_SMBIOS_BASEBOARD_VERSION);
	t->asset_tag_number = smbios_add_prop(t->eos, ctx, "asset-tag");
	t->feature_flags = SMBIOS_BOARD_FEATURE_HOSTING;
	t->board_type = SMBIOS_BOARD_MOTHERBOARD;

	len = t->length + smbios_string_table_len(t->eos);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type3(ulong *current, int handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type3 *t;
	int len = sizeof(struct smbios_type3);

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type3));
	fill_smbios_header(t, SMBIOS_SYSTEM_ENCLOSURE, len, handle);
	t->manufacturer = smbios_add_prop(t->eos, ctx, "manufacturer");
	t->chassis_type = SMBIOS_ENCLOSURE_DESKTOP;
	t->bootup_state = SMBIOS_STATE_SAFE;
	t->power_supply_state = SMBIOS_STATE_SAFE;
	t->thermal_state = SMBIOS_STATE_SAFE;
	t->security_status = SMBIOS_SECURITY_NONE;

	len = t->length + smbios_string_table_len(t->eos);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static void smbios_write_type4_dm(struct smbios_type4 *t,
				  struct smbios_ctx *ctx)
{
	u16 processor_family = SMBIOS_PROCESSOR_FAMILY_UNKNOWN;
	const char *vendor = "Unknown";
	const char *name = "Unknown";

#ifdef CONFIG_CPU
	char processor_name[49];
	char vendor_name[49];
	struct udevice *cpu = NULL;

	uclass_find_first_device(UCLASS_CPU, &cpu);
	if (cpu) {
		struct cpu_platdata *plat = dev_get_parent_platdata(cpu);

		if (plat->family)
			processor_family = plat->family;
		t->processor_id[0] = plat->id[0];
		t->processor_id[1] = plat->id[1];

		if (!cpu_get_vendor(cpu, vendor_name, sizeof(vendor_name)))
			vendor = vendor_name;
		if (!cpu_get_desc(cpu, processor_name, sizeof(processor_name)))
			name = processor_name;
	}
#endif

	t->processor_family = processor_family;
	t->processor_manufacturer = smbios_add_string(t->eos, vendor);
	t->processor_version = smbios_add_string(t->eos, name);
}

static int smbios_write_type4(ulong *current, int handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type4 *t;
	int len = sizeof(struct smbios_type4);

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type4));
	fill_smbios_header(t, SMBIOS_PROCESSOR_INFORMATION, len, handle);
	t->processor_type = SMBIOS_PROCESSOR_TYPE_CENTRAL;
	smbios_write_type4_dm(t, ctx);
	t->status = SMBIOS_PROCESSOR_STATUS_ENABLED;
	t->processor_upgrade = SMBIOS_PROCESSOR_UPGRADE_NONE;
	t->l1_cache_handle = 0xffff;
	t->l2_cache_handle = 0xffff;
	t->l3_cache_handle = 0xffff;
	t->processor_family2 = t->processor_family;

	len = t->length + smbios_string_table_len(t->eos);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type32(ulong *current, int handle,
			       struct smbios_ctx *ctx)
{
	struct smbios_type32 *t;
	int len = sizeof(struct smbios_type32);

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type32));
	fill_smbios_header(t, SMBIOS_SYSTEM_BOOT_INFORMATION, len, handle);

	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type127(ulong *current, int handle,
				struct smbios_ctx *ctx)
{
	struct smbios_type127 *t;
	int len = sizeof(struct smbios_type127);

	t = map_sysmem(*current, len);
	memset(t, 0, sizeof(struct smbios_type127));
	fill_smbios_header(t, SMBIOS_END_OF_TABLE, len, handle);

	*current += len;
	unmap_sysmem(t);

	return len;
}

static struct smbios_write_method smbios_write_funcs[] = {
	{ smbios_write_type0, },
	{ smbios_write_type1, "system", },
	{ smbios_write_type2, "baseboard", },
	{ smbios_write_type3, "chassis", },
	{ smbios_write_type4, },
	{ smbios_write_type32, },
	{ smbios_write_type127 },
};

ulong write_smbios_table(ulong addr)
{
	ofnode parent_node = ofnode_null();
	struct smbios_entry *se;
	struct smbios_ctx ctx;
	ulong table_addr;
	ulong tables;
	int len = 0;
	int max_struct_size = 0;
	int handle = 0;
	char *istart;
	int isize;
	int i;

	ctx.node = ofnode_null();
	if (IS_ENABLED(CONFIG_OF_CONTROL)) {
		uclass_first_device(UCLASS_SYSINFO, &ctx.dev);
		if (ctx.dev) {
			parent_node = dev_read_subnode(ctx.dev, "smbios");
		}
	}

	/* 16 byte align the table address */
	addr = ALIGN(addr, 16);

	se = map_sysmem(addr, sizeof(struct smbios_entry));
	memset(se, 0, sizeof(struct smbios_entry));

	addr += sizeof(struct smbios_entry);
	addr = ALIGN(addr, 16);
	tables = addr;

	/* populate minimum required tables */
	for (i = 0; i < ARRAY_SIZE(smbios_write_funcs); i++) {
		const struct smbios_write_method *method;
		int tmp;

		method = &smbios_write_funcs[i];
		if (IS_ENABLED(CONFIG_OF_CONTROL) && method->subnode_name)
			ctx.node = ofnode_find_subnode(parent_node,
						       method->subnode_name);
		tmp = method->write((ulong *)&addr, handle++, &ctx);

		max_struct_size = max(max_struct_size, tmp);
		len += tmp;
	}

	memcpy(se->anchor, "_SM_", 4);
	se->length = sizeof(struct smbios_entry);
	se->major_ver = SMBIOS_MAJOR_VER;
	se->minor_ver = SMBIOS_MINOR_VER;
	se->max_struct_size = max_struct_size;
	memcpy(se->intermediate_anchor, "_DMI_", 5);
	se->struct_table_length = len;

	/*
	 * We must use a pointer here so things work correctly on sandbox. The
	 * user of this table is not aware of the mapping of addresses to
	 * sandbox's DRAM buffer.
	 */
	table_addr = (ulong)map_sysmem(tables, 0);
	if (sizeof(table_addr) > sizeof(u32) && table_addr > (ulong)UINT_MAX) {
		/*
		 * We need to put this >32-bit pointer into the table but the
		 * field is only 32 bits wide.
		 */
		printf("WARNING: SMBIOS table_address overflow %llx\n",
		       (unsigned long long)table_addr);
		table_addr = 0;
	}
	se->struct_table_address = table_addr;

	se->struct_count = handle;

	/* calculate checksums */
	istart = (char *)se + SMBIOS_INTERMEDIATE_OFFSET;
	isize = sizeof(struct smbios_entry) - SMBIOS_INTERMEDIATE_OFFSET;
	se->intermediate_checksum = table_compute_checksum(istart, isize);
	se->checksum = table_compute_checksum(se, sizeof(struct smbios_entry));
	unmap_sysmem(se);

	return addr;
}
