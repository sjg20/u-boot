// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, <daniel@omicron.se>
 */

/*
 * Linux x86 zImage and bzImage loading
 *
 * based on the procdure described in
 * linux/Documentation/i386/boot.txt
 */

#define LOG_DEBUG

#include <common.h>
#include <command.h>
#include <env.h>
#include <init.h>
#include <irq_func.h>
#include <log.h>
#include <malloc.h>
#include <acpi/acpi_table.h>
#include <asm/intel_gnvs.h>
#include <asm/io.h>
#include <asm/ptrace.h>
#include <asm/zimage.h>
#include <asm/byteorder.h>
#include <asm/bootm.h>
#include <asm/bootparam.h>
#include <linux/compiler.h>
#include <linux/libfdt.h>

/*
 * Memory lay-out:
 *
 * relative to setup_base (which is 0x90000 currently)
 *
 *	0x0000-0x7FFF	Real mode kernel
 *	0x8000-0x8FFF	Stack and heap
 *	0x9000-0x90FF	Kernel command line
 */
#define DEFAULT_SETUP_BASE	0x90000
#define COMMAND_LINE_OFFSET	0x9000
#define HEAP_END_OFFSET		0x8e00

#define COMMAND_LINE_SIZE	2048

struct zboot_state {
	ulong bzimage_addr;
	ulong bzimage_size;
	struct boot_params *base_ptr;
	ulong initrd_addr;
	ulong initrd_size;
	ulong load_address;
	ulong cmdline;
} state;

enum {
	ZBOOT_STATE_START	= BIT(0),
	ZBOOT_STATE_LOAD	= BIT(1),
	ZBOOT_STATE_SETUP	= BIT(2),
	ZBOOT_STATE_INFO	= BIT(3),
	ZBOOT_STATE_GO		= BIT(4),

	/* This one doesn't execute automatically, so stop the count before 5 */
	ZBOOT_STATE_DUMP	= BIT(5),
	ZBOOT_STATE_COUNT	= 5,
};

static void build_command_line(char *command_line, int auto_boot)
{
	char *env_command_line;

	command_line[0] = '\0';

	env_command_line =  env_get("bootargs");

	/* set console= argument if we use a serial console */
	if (!strstr(env_command_line, "console=")) {
		if (!strcmp(env_get("stdout"), "serial")) {

			/* We seem to use serial console */
			sprintf(command_line, "console=ttyS0,%s ",
				env_get("baudrate"));
		}
	}

	if (auto_boot)
		strcat(command_line, "auto ");

	if (env_command_line)
		strcat(command_line, env_command_line);
}

static int kernel_magic_ok(struct setup_header *hdr)
{
	if (KERNEL_MAGIC != hdr->boot_flag) {
		printf("Error: Invalid Boot Flag "
			"(found 0x%04x, expected 0x%04x)\n",
			hdr->boot_flag, KERNEL_MAGIC);
		return 0;
	} else {
		printf("Valid Boot Flag\n");
		return 1;
	}
}

static int get_boot_protocol(struct setup_header *hdr, bool verbose)
{
	if (hdr->header == KERNEL_V2_MAGIC) {
		if (verbose)
			printf("Magic signature found\n");
		return hdr->version;
	} else {
		/* Very old kernel */
		if (verbose)
			printf("Magic signature not found\n");
		return 0x0100;
	}
}

static int setup_device_tree(struct setup_header *hdr, const void *fdt_blob)
{
	int bootproto = get_boot_protocol(hdr, false);
	struct setup_data *sd;
	int size;

	if (bootproto < 0x0209)
		return -ENOTSUPP;

	if (!fdt_blob)
		return 0;

	size = fdt_totalsize(fdt_blob);
	if (size < 0)
		return -EINVAL;

	size += sizeof(struct setup_data);
	sd = (struct setup_data *)malloc(size);
	if (!sd) {
		printf("Not enough memory for DTB setup data\n");
		return -ENOMEM;
	}

	sd->next = hdr->setup_data;
	sd->type = SETUP_DTB;
	sd->len = fdt_totalsize(fdt_blob);
	memcpy(sd->data, fdt_blob, sd->len);
	hdr->setup_data = (unsigned long)sd;

	return 0;
}

static const char *get_kernel_version(struct boot_params *params,
				      void *kernel_base)
{
	struct setup_header *hdr = &params->hdr;
	int bootproto;

	bootproto = get_boot_protocol(hdr, false);
	if (bootproto < 0x0200 || hdr->setup_sects < 15)
		return NULL;

	return kernel_base + hdr->kernel_version + 0x200;
	}


struct boot_params *load_zimage(char *image, unsigned long kernel_size,
				ulong *load_addressp)
{
	struct boot_params *setup_base;
	const char *version;
	int setup_size;
	int bootproto;
	int big_image;

	struct boot_params *params = (struct boot_params *)image;
	struct setup_header *hdr = &params->hdr;

	/* base address for real-mode segment */
	setup_base = (struct boot_params *)DEFAULT_SETUP_BASE;

	if (!kernel_magic_ok(hdr))
		return 0;

	/* determine size of setup */
	if (0 == hdr->setup_sects) {
		printf("Setup Sectors = 0 (defaulting to 4)\n");
		setup_size = 5 * 512;
	} else {
		setup_size = (hdr->setup_sects + 1) * 512;
	}

	printf("Setup Size = 0x%8.8lx\n", (ulong)setup_size);

	if (setup_size > SETUP_MAX_SIZE)
		printf("Error: Setup is too large (%d bytes)\n", setup_size);

	/* determine boot protocol version */
	bootproto = get_boot_protocol(hdr, true);

	printf("Using boot protocol version %x.%02x\n",
	       (bootproto & 0xff00) >> 8, bootproto & 0xff);

	version = get_kernel_version(params, image);
	if (version)
		printf("Linux kernel version %s\n", version);
	else
		printf("Setup Sectors < 15 - Cannot print kernel version\n");

	/* Determine image type */
	big_image = (bootproto >= 0x0200) &&
		    (hdr->loadflags & BIG_KERNEL_FLAG);

	/* Determine load address */
	if (big_image)
		*load_addressp = BZIMAGE_LOAD_ADDR;
	else
		*load_addressp = ZIMAGE_LOAD_ADDR;
// 	*load_addressp = 0x01800000;

	printf("Building boot_params at 0x%8.8lx\n", (ulong)setup_base);
	memset(setup_base, 0, sizeof(*setup_base));
	setup_base->hdr = params->hdr;

	if (bootproto >= 0x0204)
		kernel_size = hdr->syssize * 16;
	else
		kernel_size -= setup_size;

	if (bootproto == 0x0100) {
		/*
		 * A very old kernel MUST have its real-mode code
		 * loaded at 0x90000
		 */
		if ((ulong)setup_base != 0x90000) {
			/* Copy the real-mode kernel */
			memmove((void *)0x90000, setup_base, setup_size);

			/* Copy the command line */
			memmove((void *)0x99000,
				(u8 *)setup_base + COMMAND_LINE_OFFSET,
				COMMAND_LINE_SIZE);

			 /* Relocated */
			setup_base = (struct boot_params *)0x90000;
		}

		/* It is recommended to clear memory up to the 32K mark */
		memset((u8 *)0x90000 + setup_size, 0,
		       SETUP_MAX_SIZE - setup_size);
	}

	if (big_image) {
		if (kernel_size > BZIMAGE_MAX_SIZE) {
			printf("Error: bzImage kernel too big! "
				"(size: %ld, max: %d)\n",
				kernel_size, BZIMAGE_MAX_SIZE);
			return 0;
		}
	} else if ((kernel_size) > ZIMAGE_MAX_SIZE) {
		printf("Error: zImage kernel too big! (size: %ld, max: %d)\n",
		       kernel_size, ZIMAGE_MAX_SIZE);
		return 0;
	}

	printf("Loading %s at address %lx (%ld bytes)\n",
	       big_image ? "bzImage" : "zImage", *load_addressp, kernel_size);

	memmove((void *)*load_addressp, image + setup_size, kernel_size);

	return setup_base;
}

static void add_entry(struct e820_entry *entries, int pos, u64 addr, u64 size,
		      uint type)
{
	struct e820_entry *entry = &entries[pos];

	entry->addr = addr;
	entry->size = size;
	entry->type = type;
}

static unsigned int do_install_e820_map(unsigned int max_entries,
					struct e820_entry *entries)
{
	int i;

	i = 0;
	add_entry(entries, i++, 0, 0x1000, E820_RESERVED);
	add_entry(entries, i++, 0x1000, 0x9f000, E820_RAM);
	add_entry(entries, i++, 0xa0000, 0x60000, E820_RESERVED);
	add_entry(entries, i++, 0x100000, 0xff00000, E820_RAM);
	add_entry(entries, i++, 0x10000000, 0x2151000, E820_RESERVED);

	add_entry(entries, i++, 0x12151000, 0x6888d000, E820_RAM);
	add_entry(entries, i++, 0x7a9de000, 0x622000, E820_RESERVED);
	add_entry(entries, i++, 0x7b000000, 0x5000000, E820_RESERVED);
	add_entry(entries, i++, 0xd0000000, 0x1000000, E820_RESERVED);
	add_entry(entries, i++, 0xe0000000, 0x10000000, E820_RESERVED);

	add_entry(entries, i++, 0xfe042000, 0x2000, E820_RESERVED);
	add_entry(entries, i++, 0xfed10000, 0x8000, E820_RESERVED);
	add_entry(entries, i++, 0x100000000, 0x80000000, E820_RAM);

	return i;
}

enum {
	CHSW_RECOVERY_X86 =		BIT(1),
	CHSW_RECOVERY_EC =		BIT(2),
	CHSW_DEVELOPER_SWITCH =		BIT(5),
	CHSW_FIRMWARE_WP =		BIT(9),
};

enum {
	FIRMWARE_TYPE_AUTO_DETECT = -1,
	FIRMWARE_TYPE_RECOVERY = 0,
	FIRMWARE_TYPE_NORMAL = 1,
	FIRMWARE_TYPE_DEVELOPER = 2,
	FIRMWARE_TYPE_NETBOOT = 3,
	FIRMWARE_TYPE_LEGACY = 4,
};

#define ACPI_FWID_SIZE		64

static void write_chromos_acpi(void)
{
	struct chromeos_acpi *tab;
	char *ptr;

	tab = (struct chromeos_acpi *)0x7ab2d100;
	memset(tab, '\0', sizeof(*tab));
	tab->vbt0 = 0;
	tab->vbt1 = 1;
	tab->vbt2 = 1;
	tab->vbt3 = CHSW_RECOVERY_EC | CHSW_FIRMWARE_WP;
	strcpy((char *)tab->vbt4, "CORAL TEST 8594");
	strcpy((char *)tab->vbt5, "Google_Coral.13074.0.2020_05_30_1642");
	strcpy((char *)tab->vbt6, "Google_Coral.13074.0.2020_05_30_1642");
	tab->vbt7 = FIRMWARE_TYPE_DEVELOPER;
	tab->vbt8 = 0;
	tab->vbt9 = 0x7abdd000;
// 	log_debug("FMAP:\n");
// 	print_buffer(tab->vbt9, (void *)tab->vbt9, 1, 0x100, 0);
	tab->vbt10 = 0x7a9de04e;
	ptr = (char *)tab->vbt10;
// 	log_debug("FWID before:\n");
// 	print_buffer((ulong)ptr, ptr, 1, ACPI_FWID_SIZE, 0);
	memset(ptr, ' ', ACPI_FWID_SIZE);
	strcpy(ptr, "Google_Coral.13074.0.2020_05_30_1642");
	log_debug("FWID:\n");
	print_buffer((ulong)ptr, ptr, 1, ACPI_FWID_SIZE, 0);
// 	log_debug("contents:\n");
// 	print_buffer((ulong)tab, tab, 1, sizeof(*tab), 0);

}

int setup_zimage(struct boot_params *setup_base, char *cmd_line, int auto_boot,
		 ulong initrd_addr, ulong initrd_size, ulong cmdline_force)
{
	struct setup_header *hdr = &setup_base->hdr;
	int bootproto = get_boot_protocol(hdr, false);

	log_debug("Setup E820 entries\n");
	setup_base->e820_entries = do_install_e820_map(
		ARRAY_SIZE(setup_base->e820_map), setup_base->e820_map);

	log_debug("Write Chrome OS stuff\n");
	write_chromos_acpi();

	if (bootproto == 0x0100) {
		setup_base->screen_info.cl_magic = COMMAND_LINE_MAGIC;
		setup_base->screen_info.cl_offset = COMMAND_LINE_OFFSET;
	}
	if (bootproto >= 0x0200) {
		hdr->type_of_loader = 0x80;	/* U-Boot version 0 */
		if (initrd_addr) {
			printf("Initial RAM disk at linear address "
			       "0x%08lx, size %ld bytes\n",
			       initrd_addr, initrd_size);

			hdr->ramdisk_image = initrd_addr;
			hdr->ramdisk_size = initrd_size;
		}
	}

	if (bootproto >= 0x0201) {
		hdr->heap_end_ptr = HEAP_END_OFFSET;
		hdr->loadflags |= HEAP_FLAG;
	}

	if (cmd_line) {
		log_debug("Setup cmdline\n");
		if (bootproto >= 0x0202) {
			hdr->cmd_line_ptr = (uintptr_t)cmd_line;
		} else if (bootproto >= 0x0200) {
			setup_base->screen_info.cl_magic = COMMAND_LINE_MAGIC;
			setup_base->screen_info.cl_offset =
				(uintptr_t)cmd_line - (uintptr_t)setup_base;

			hdr->setup_move_size = 0x9100;
		}

		/* build command line at COMMAND_LINE_OFFSET */
		if (cmdline_force)
			strcpy(cmd_line, (char *)cmdline_force);
		else
			build_command_line(cmd_line, auto_boot);
		strcpy(cmd_line, "console= loglevel=7 init=/sbin/init oops=panic panic=-1 root=PARTUUID=35c775e7-3735-d745-93e5-d9e0238f7ed0/PARTNROFF=1 rootwait rw noinitrd vt.global_cursor_default=0 add_efi_memmap boot=local noresume noswap i915.modeset=1 nmi_watchdog=panic,lapic disablevmx=off");
		printf("Kernel command line: \"");
		puts(cmd_line);
		printf("\"\n");
	}

	if (IS_ENABLED(CONFIG_INTEL_MID) && bootproto >= 0x0207)
		hdr->hardware_subarch = X86_SUBARCH_INTEL_MID;

	if (IS_ENABLED(CONFIG_GENERATE_ACPI_TABLE))
		setup_base->acpi_rsdp_addr = acpi_get_rsdp_addr();

	log_debug("Setup devicetree\n");
	setup_device_tree(hdr, (const void *)env_get_hex("fdtaddr", 0));
	setup_video(&setup_base->screen_info);

	if (IS_ENABLED(CONFIG_EFI_STUB)) {
		log_debug("Setup EFI\n");
		setup_efi_info(&setup_base->efi_info);
	}

	return 0;
}

int do_zboot_start(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	const char *s;

	memset(&state, '\0', sizeof(state));
	if (argc >= 2) {
		/* argv[1] holds the address of the bzImage */
		s = argv[1];
	} else {
		s = env_get("fileaddr");
	}

	if (s)
		state.bzimage_addr = simple_strtoul(s, NULL, 16);

	if (argc >= 3) {
		/* argv[2] holds the size of the bzImage */
		state.bzimage_size = simple_strtoul(argv[2], NULL, 16);
	}

	if (argc >= 4)
		state.initrd_addr = simple_strtoul(argv[3], NULL, 16);
	if (argc >= 5)
		state.initrd_size = simple_strtoul(argv[4], NULL, 16);
	if (argc >= 6) {
		state.base_ptr = (void *)simple_strtoul(argv[5], NULL, 16);
		state.load_address = state.bzimage_addr;
		state.bzimage_addr = 0;
	}
	if (argc >= 7)
		state.cmdline = simple_strtoul(argv[6], NULL, 16);

	return 0;
}

int do_zboot_load(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct boot_params *base_ptr;

	if (state.base_ptr) {
		struct boot_params *from = (struct boot_params *)state.base_ptr;

		base_ptr = (struct boot_params *)DEFAULT_SETUP_BASE;
		printf("Building boot_params at 0x%8.8lx\n", (ulong)base_ptr);
		memset(base_ptr, '\0', sizeof(*base_ptr));
		base_ptr->hdr = from->hdr;
	} else {
		base_ptr = load_zimage((void *)state.bzimage_addr, state.bzimage_size,
				       &state.load_address);
		if (!base_ptr) {
			puts("## Kernel loading failed ...\n");
			return CMD_RET_FAILURE;
		}
	}
	state.base_ptr = base_ptr;
	if (env_set_hex("zbootbase", (ulong)base_ptr) ||
	    env_set_hex("zbootaddr", state.load_address))
		return CMD_RET_FAILURE;

	return 0;
}

int do_zboot_setup(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	struct boot_params *base_ptr = state.base_ptr;
	int ret;

	if (!base_ptr) {
		printf("base is not set: use 'zboot load' first\n");
		return CMD_RET_FAILURE;
	}
	ret = setup_zimage(base_ptr, (char *)base_ptr + COMMAND_LINE_OFFSET,
			   0, state.initrd_addr, state.initrd_size,
			   state.cmdline);
	if (ret) {
		puts("Setting up boot parameters failed ...\n");
		return CMD_RET_FAILURE;
	}

	return 0;
}

int do_zboot_info(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	printf("Kernel loaded at %08lx, setup_base=%p\n",
	       state.load_address, state.base_ptr);

	return 0;
}

int do_zboot_go(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
// 	struct boot_params *base_ptr = state.base_ptr;
// 	struct setup_header *hdr = &base_ptr->hdr;
	int ret;

// 	printf("forcing base\n");
// 	state.base_ptr = (void *)0x1000;
// 	state.load_address = 0x100000;
// 	printf("forcing cmdline\n");
// 	hdr->cmd_line_ptr = 0x2000;

	disable_interrupts();

	printf("Booting kernel at %lx, base_ptr=%p, ll_boot_init()=%d\n",
	       state.load_address, state.base_ptr, ll_boot_init());

	/* we assume that the kernel is in place */
	ret = boot_linux_kernel((ulong)state.base_ptr, state.load_address,
				false);
	printf("Kernel returned! (err=%d)\n", ret);

	return 0;
}

static void print_num(const char *name, ulong value)
{
	printf("%-20s: %lx\n", name, value);
}

static void print_num64(const char *name, u64 value)
{
	printf("%-20s: %llx\n", name, value);
}

static const char *const e820_type_name[E820_COUNT] = {
	[E820_RAM] = "RAM",
	[E820_RESERVED] = "Reserved",
	[E820_ACPI] = "ACPI",
	[E820_NVS] = "ACPI NVS",
	[E820_UNUSABLE] = "Unusable",
};

static const char *const bootloader_id[] = {
	"LILO",
	"Loadlin",
	"bootsect-loader",
	"Syslinux",
	"Etherboot/gPXE/iPXE",
	"ELILO",
	"undefined",
	"GRUB",
	"U-Boot",
	"Xen",
	"Gujin",
	"Qemu",
	"Arcturus Networks uCbootloader",
	"kexec-tools",
	"Extended",
	"Special",
	"Reserved",
	"Minimal Linux Bootloader",
	"OVMF UEFI virtualization stack",
};

struct flag_info {
	uint bit;
	const char *name;
};

struct flag_info load_flags[] = {
	{ LOADED_HIGH, "loaded-high" },
	{ QUIET_FLAG, "quiet" },
	{ KEEP_SEGMENTS, "keep-segments" },
	{ CAN_USE_HEAP, "can-use-heap" },
};

struct flag_info xload_flags[] = {
	{ XLF_KERNEL_64, "64-bit-entry" },
	{ XLF_CAN_BE_LOADED_ABOVE_4G, "can-load-above-4gb" },
	{ XLF_EFI_HANDOVER_32, "32-efi-handoff" },
	{ XLF_EFI_HANDOVER_64, "64-efi-handoff" },
	{ XLF_EFI_KEXEC, "kexec-efi-runtime" },
};

static void print_flags(struct flag_info *flags, int count, uint value)
{
	int i;

	printf("%-20s:", "");
	for (i = 0; i < count; i++) {
		uint mask = flags[i].bit;

		if (value & mask)
			printf(" %s", flags[i].name);
	}
	printf("\n");
}

static void show_loader(struct setup_header *hdr)
{
	bool version_valid = false;
	int type, version;
	const char *name;

	type = hdr->type_of_loader >> 4;
	version = hdr->type_of_loader & 0xf;
	if (type == 0xe)
		type = 0x10 + hdr->ext_loader_type;
	version |= hdr->ext_loader_ver << 4;
	if (!hdr->type_of_loader) {
		name = "pre-2.00 bootloader";
	} else if (hdr->type_of_loader == 0xff) {
		name = "unknown";
	} else if (type < ARRAY_SIZE(bootloader_id)) {
		name = bootloader_id[type];
		version_valid = true;
	} else {
		name = "undefined";
	}
	printf("%20s  %s", "", name);
	if (version_valid)
		printf(", version %x", version);
	printf("\n");
}

int do_zboot_dump(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct boot_params *base_ptr = state.base_ptr;
	struct setup_header *hdr;
	const char *version;
	int i;

	if (argc > 1)
		base_ptr = (void *)simple_strtoul(argv[1], NULL, 16);
	if (!base_ptr) {
		printf("No zboot setup_base\n");
		return CMD_RET_FAILURE;
	}
	printf("Setup located at %p:\n\n", base_ptr);
	print_num64("ACPI RSDP addr", base_ptr->acpi_rsdp_addr);

	printf("E820: %d entries\n", base_ptr->e820_entries);
	if (base_ptr->e820_entries) {
		printf("%18s  %16s  %s\n", "Addr", "Size", "Type");
		for (i = 0; i < base_ptr->e820_entries; i++) {
			struct e820_entry *entry = &base_ptr->e820_map[i];

			printf("%12llx  %10llx  %s\n", entry->addr, entry->size,
			       entry->type < E820_COUNT ?
			       e820_type_name[entry->type] :
			       simple_itoa(entry->type));
		}
	}

	hdr = &base_ptr->hdr;
	print_num("Setup sectors", hdr->setup_sects);
	print_num("Root flags", hdr->root_flags);
	print_num("Sys size", hdr->syssize);
	print_num("RAM size", hdr->ram_size);
	print_num("Video mode", hdr->vid_mode);
	print_num("Root dev", hdr->root_dev);
	print_num("Boot flag", hdr->boot_flag);
	print_num("Jump", hdr->jump);
	print_num("Header", hdr->header);
	if (hdr->header == KERNEL_V2_MAGIC)
		printf("%-20s  %s\n", "", "Kernel V2");
	else
		printf("%-20s  %s\n", "", "Ancient kernel, using version 100");
	print_num("Version", hdr->version);
	print_num("Real mode switch", hdr->realmode_swtch);
	print_num("Start sys", hdr->start_sys);
	print_num("Kernel version", hdr->kernel_version);
	version = get_kernel_version(base_ptr, (void *)state.bzimage_addr);
	if (version)
		printf("   @%p: %s\n", version, version);
	print_num("Type of loader", hdr->type_of_loader);
	show_loader(hdr);
	print_num("Load flags", hdr->loadflags);
	print_flags(load_flags, ARRAY_SIZE(load_flags), hdr->loadflags);
	print_num("Setup move size", hdr->setup_move_size);
	print_num("Code32 start", hdr->code32_start);
	print_num("Ramdisk image", hdr->ramdisk_image);
	print_num("Ramdisk size", hdr->ramdisk_size);
	print_num("Bootsect kludge", hdr->bootsect_kludge);
	print_num("Heap end ptr", hdr->heap_end_ptr);
	print_num("Ext loader ver", hdr->ext_loader_ver);
	print_num("Ext loader type", hdr->ext_loader_type);
	print_num("Commandline ptr", hdr->cmd_line_ptr);
	if (hdr->cmd_line_ptr) {
		printf("   ");
		/* Use puts() to avoid limits from CONFIG_SYS_PBSIZE */
		puts((char *)hdr->cmd_line_ptr);
		printf("\n");
	}
	print_num("Initrd addr max", hdr->initrd_addr_max);
	print_num("Kernel alignment", hdr->kernel_alignment);
	print_num("Relocatable kernel", hdr->relocatable_kernel);
	print_num("Min alignment", hdr->min_alignment);
	if (hdr->min_alignment)
		printf("%-20s: %x\n", "", 1 << hdr->min_alignment);
	print_num("Xload flags", hdr->xloadflags);
	print_flags(xload_flags, ARRAY_SIZE(xload_flags), hdr->xloadflags);
	print_num("Cmdline size", hdr->cmdline_size);
	print_num("Hardware subarch", hdr->hardware_subarch);
	print_num64("HW subarch data", hdr->hardware_subarch_data);
	print_num("Payload offset", hdr->payload_offset);
	print_num("Payload length", hdr->payload_length);
	print_num64("Setup data", hdr->setup_data);
	print_num64("Pref address", hdr->pref_address);
	print_num("Init size", hdr->init_size);
	print_num("Handover offset", hdr->handover_offset);
	if (get_boot_protocol(hdr, false) >= 0x215)
		print_num("Kernel info offset", hdr->kernel_info_offset);

	return 0;
}

U_BOOT_SUBCMDS(zboot,
	U_BOOT_CMD_MKENT(start, 8, 1, do_zboot_start, "", ""),
	U_BOOT_CMD_MKENT(load, 1, 1, do_zboot_load, "", ""),
	U_BOOT_CMD_MKENT(setup, 1, 1, do_zboot_setup, "", ""),
	U_BOOT_CMD_MKENT(info, 1, 1, do_zboot_info, "", ""),
	U_BOOT_CMD_MKENT(go, 1, 1, do_zboot_go, "", ""),
	U_BOOT_CMD_MKENT(dump, 2, 1, do_zboot_dump, "", ""),
)

int do_zboot_states(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[], int state_mask)
{
	int i;

	for (i = 0; i < ZBOOT_STATE_COUNT; i++) {
		struct cmd_tbl *cmd = &zboot_subcmds[i];
		int mask = 1 << i;
		int ret;

		if (mask & state_mask) {
			ret = cmd->cmd(cmd, flag, argc, argv);
			if (ret)
				return ret;
		}
	}

	return 0;
}

int do_zboot_parent(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[], int *repeatable)
{
	/* determine if we have a sub command */
	if (argc > 1) {
		char *endp;

		simple_strtoul(argv[1], &endp, 16);
		/*
		 * endp pointing to nul means that argv[0] was just a valid
		 * number, so pass it along to the normal processing
		 */
		if (*endp)
			return do_zboot(cmdtp, flag, argc, argv, repeatable);
	}

	do_zboot_states(cmdtp, flag, argc, argv, ZBOOT_STATE_START |
			ZBOOT_STATE_LOAD | ZBOOT_STATE_SETUP |
			ZBOOT_STATE_INFO | ZBOOT_STATE_GO);

	return CMD_RET_FAILURE;
}

U_BOOT_CMDREP_COMPLETE(
	zboot, 8, do_zboot_parent, "Boot bzImage",
	"[addr] [size] [initrd addr] [initrd size] [setup]\n"
	"      addr -        The optional starting address of the bzimage.\n"
	"                    If not set it defaults to the environment\n"
	"                    variable \"fileaddr\".\n"
	"      size -        The optional size of the bzimage. Defaults to\n"
	"                    zero.\n"
	"      initrd addr - The address of the initrd image to use, if any.\n"
	"      initrd size - The size of the initrd image to use, if any.\n"
	"      setup -       The address of the kernel setup region, if this\n"
	"                    is not at addr\n"
	"      cmdline -     The address of the kernel command line, to\n"
	"                    override U-Boot's normal cmdline generation\n"
	"\n"
	"Sub-commands to do part of the zboot sequence:\n"
	"\tstart [addr [arg ...]] - specify arguments\n"
	"\tload   - load OS image\n"
	"\tsetup  - set up table\n"
	"\tinfo   - show sumary info\n"
	"\tgo     - start OS\n"
	"\tdump [addr]    - dump info (optional address of boot params)",
	complete_zboot
);
