/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * UPL handoff generation
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __UPL_WRITE_H
#define __UPL_WRITE_H

#include <dm/ofnode_decl.h>

/**
 * Constans to control the UPL-implementation limits
 *
 * @UPL_MAX_IMAGES: Maximum number of images allowed
 * @UPL_MAX_MEMS: Maximum number of /memory-xxx entries
 * @UPL_MAX_MEMREGIONS: Maximum number of memory regions in each /menory entry
 */
enum {
	UPL_MAX_IMAGES		= 8,
	UPL_MAX_MEMS		= 8,
	UPL_MAX_MEMREGIONS	= 8,
	UPL_MAX_MEMMAPS		= 32,
	UPL_MAX_MEMRESERVED	= 8,
};

#define UPLP_ADDRESS_CELLS	"#address-cells"
#define UPLP_SIZE_CELLS		"#size-cells"

#define UPLN_OPTIONS		"options"
#define UPLN_UPL_PARAMS		"upl-params"
#define UPLP_SMBIOS		"smbios"
#define UPLP_ACPI		"acpi"
#define UPLP_BOOTMODE		"bootmode"
#define UPLP_ADDR_WIDTH		"addr-width"
#define UPLP_ACPI_NVS_SIZE	"acpi-nvs-size"

#define UPLPATH_UPL_IMAGE	"/options/upl-image"
#define UPLN_UPL_IMAGE		"upl-image"
#define UPLN_IMAGE		"image"
#define UPLP_FIT		"fit"
#define UPLP_CONF_OFFSET	"conf-offset"
#define UPLP_LOAD		"load"
#define UPLP_SIZE		"size"
#define UPLP_OFFSET		"offset"
#define UPLP_DESCRIPTION	"description"

#define UPLN_MEMORY		"memory"
#define UPLP_HOTPLUGGABLE	"hotpluggable"

#define UPLPATH_MEMORY_MAP	"/memory-map"
#define UPLN_MEMORY_MAP		"memory-map"
#define UPLP_USAGE		"usage"

#define UPLN_MEMORY_RESERVED	"reserved-memory"
#define UPLPATH_MEMORY_RESERVED	"/reserved-memory"
#define UPLP_NO_MAP		"no-map"

#define UPLN_SERIAL		"serial"
#define UPLP_REG		"reg"
#define UPLP_COMPATIBLE		"compatible"
#define UPLP_CLOCK_FREQUENCY	"clock-frequency"
#define UPLP_CURRENT_SPEED	"current-speed"
#define UPLP_REG_IO_SHIFT	"reg-io-shift"
#define UPLP_REG_OFFSET		"reg-offset"
#define UPLP_REG_IO_WIDTH	"reg-io-width"
#define UPLP_VIRTUAL_REG	"virtual-reg"
#define UPLP_ACCESS_TYPE	"access-type"

#define UPLN_GRAPHICS		"framebuffer"
#define UPLC_GRAPHICS		"simple-framebuffer"
#define UPLP_WIDTH		"width"
#define UPLP_HEIGHT		"height"
#define UPLP_STRIDE		"stride"
#define UPLP_GRAPHICS_FORMAT	"format"

/**
 * enum upl_boot_mode - Encodes the boot mode
 *
 * Each is a bit number from the boot_mode mask
 */
enum upl_boot_mode {
	UPLBM_FULL,
	UPLBM_MINIMAL,
	UPLBM_FAST,
	UPLBM_DIAG,
	UPLBM_DEFAULT,
	UPLBM_S2,
	UPLBM_S3,
	UPLBM_S4,
	UPLBM_S5,
	UPLBM_FACTORY,
	UPLBM_FLASH,
	UPLBM_RECOVERY,

	UPLBM_COUNT,
};

/**
 * struct upl_image - UPL image informaiton
 *
 * @load: Address image was loaded to
 * @size: Size of image in bytes
 * @offset: Offset of the image in the FIT (0=none)
 * @desc: Description of the iamge (taken from the FIT)
 */
struct upl_image {
	ulong load;
	ulong size;
	uint offset;
	const char *description;
};

/**
 * struct memregion - Information about a region of memory
 *
 * @base: Base address
 * @size: Size in bytes
 */
struct memregion {
	ulong base;
	ulong size;
};

/**
 * struct upl_mem - Information about physical-memory layout
 *
 * TODO: Figure out initial-mapped-area
 *
 * @num_regions: Number of regions
 * @region: Memory region list
 * @hotpluggable: true if hotpluggable
 */
struct upl_mem {
	uint num_regions;
	struct memregion region[UPL_MAX_MEMREGIONS];
	bool hotpluggable;
};

/**
 * enum upl_usage - Encodes the usage
 *
 * Each is a bit number from the usage mask
 */
enum upl_usage {
	UPLUS_ACPI_RECLAIM,
	UPLUS_ACPI_NVS,
	UPLUS_BOOT_CODE,
	UPLUS_BOOT_DATA,
	UPLUS_RUNTIME_CODE,
	UPLUS_RUNTIME_DATA,
	UPLUS_COUNT
};

/**
 * struct upl_memmap - Information about logical-memory layout
 *
 * @name: Node name to use
 * @num_regions: Number of regions
 * @region: Memory region list
 * @usage: Memory-usage mask (enum upl_usage)
 */
struct upl_memmap {
	const char *name;
	uint num_regions;
	struct memregion region[UPL_MAX_MEMREGIONS];
	uint usage;
};

/**
 * struct upl_memres - Reserved memory
 *
 * @name: Node name to use
 * @num_regions: Number of regions
 * @region: Reserved memory region list
 * @no_map: true to indicate that a virtual mapping must not be created
 */
struct upl_memres {
	const char *name;
	uint num_regions;
	struct memregion region[UPL_MAX_MEMREGIONS];
	bool no_map;
};

enum upl_serial_access_type {
	UPLSAT_MMIO,
	UPLSAT_IO,
};

/* serial defaults */
enum {
	UPLD_REG_IO_SHIFT	= 0,
	UPLD_REG_OFFSET		= 0,
	UPLD_REG_IO_WIDTH	= 1,
};

/**
 * enum upl_access_type - Access types
 *
 * @UPLAT_MMIO: Memory-mapped I/O
 * @UPLAT_IO: Separate I/O
 */
enum upl_access_type {
	UPLAT_MMIO,
	UPLAT_IO,
};

/**
 * struct upl_serial - Serial console
 *
 * @compatible: Compatible string (NULL if there is no serial console)
 * @clock_frequency: Input clock frequency of UART
 * @current_speed: Current baud rate of UART
 * @reg: Base address and size of registers (only one range supported)
 * @reg_shift_log2: log2 of distance between each register
 * @reg_offset: Offset of registers from the base address
 * @reg_width: Register width in bytes
 * @virtual_reg: Virtual register access (0 for none)
 * @access_type: Register access type to use
 */
struct upl_serial {
	const char *compatible;
	uint clock_frequency;
	uint current_speed;
	struct memregion reg;
	uint reg_io_shift;
	uint reg_offset;
	uint reg_io_width;
	ulong virtual_reg;
	enum upl_serial_access_type access_type;
};

/**
 * enum upl_graphics_format - Graphics formats
 *
 * @UPLGF_ARGB32: 32bpp format using 0xaarrggbb
 * @UPLGF_ABGR32: 32bpp format using 0xaabbggrr
 * @UPLGF_ARGB64: 64bpp format using 0xaaaabbbbggggrrrr
 */
enum upl_graphics_format {
	UPLGF_ARGB32,
	UPLGF_ABGR32,
	UPLGF_ABGR64,
};

struct upl_graphics {
	struct memregion reg;
	uint width;
	uint height;
	uint stride;
	enum upl_graphics_format format;
};

/*
 * Information about the UPL state
 *
 * @addr_cells: Number of address cells used in the handoff
 * @size_cells: Number of size cells used in the handoff
 * @bootmode: Boot-mode mask (enum upl_boot_mode)
 * @fit: Address of FIT image that was loaded
 * @conf_offset: Offset in FIT of the configuration that was selected
 * @addr_width: Adress-bus width of machine, e.g. 46 for 46 bits
 * @acpi_nvs_size: Size of the ACPI non-volatile-storage area in bytes
 * @num_images: Number of images
 * @image: Information about each image
 * @num_mems: Number of physical-memory regions
 * @memory: Information about physical-memory regions
 * num_memmaps: Number of logical-memory regions
 * @nennap: Information about logical-memory regions
 */
struct upl {
	int addr_cells;
	int size_cells;

	ulong smbios;
	ulong acpi;
	uint bootmode;
	ulong fit;
	uint conf_offset;
	uint addr_width;
	uint acpi_nvs_size;

	uint num_images;
	struct upl_image image[UPL_MAX_IMAGES];
	uint num_mems;
	struct upl_mem mem[UPL_MAX_MEMS];
	uint num_memmaps;
	struct upl_memmap memmap[UPL_MAX_MEMMAPS];
	uint num_memres;
	struct upl_memres memres[UPL_MAX_MEMRESERVED];
	struct upl_serial serial;
	struct upl_graphics graphics;
};

/**
 * upl_write_handoff() - Write a Unversal Payload handoff structure
 *
 * upl: UPL state to write
 * @root: root node to write it to
 * @skip_existing: Avoid recreating any nodes which already exist in the
 * devicetree. For example, if there is a serial node, just leave it alone,
 * since don't need to create a new one
 * Return: 0 on success, -ve on error
 */
int upl_write_handoff(const struct upl *upl, ofnode root, bool skip_existing);

/**
 * upl_create_handoff_tree() - Write a Unversal Payload handoff structure
 *
 * upl: UPL state to write
 * @treep: Returns a new tree containing the handoff
 * Return: 0 on success, -ve on error
 */
int upl_create_handoff_tree(const struct upl *upl, oftree *treep);

/**
 * upl_read_handoff() - Read a Unversal Payload handoff structure
 *
 * upl: UPL state to read into
 * @tree: Devicetree containing the data to read
 * Return: 0 on success, -ve on error
 */
int upl_read_handoff(struct upl *upl, oftree tree);

/* Fill a UPL with some test data */
void upl_get_test_data(struct upl *upl);

#if CONFIG_IS_ENABLED(UPL)

/**
 * upl_set_fit_info() - Set up basic info about the FIT
 *
 * @fit: Address of FIT
 * @conf_offset: Configuration node being used
 * @entry_addr: Entry address for next phase
 */
void upl_set_fit_info(ulong fit, int conf_offset, ulong entry_addr);

/**
 * upl_add_image() - Add a new image to the UPL
 *
 * @node: Image node offset in FIT
 * @load_addr: Address to which images was loaded
 * @size: Image size in bytes
 * @desc: Description of image
 */
int upl_add_image(int node, ulong load_addr, ulong size, const char *desc);

#else
static inline void upl_set_fit_info(ulong fit, int conf_offset,
				    ulong entry_addr) {}
static inline int upl_add_image(int node, ulong load_addr, ulong size,
				const char *desc)
{
	return 0;
}
#endif /* UPL */

#endif /* __UPL_WRITE_H */
