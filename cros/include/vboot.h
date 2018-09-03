/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Main verified-boot structs and functions
 *
 * Copyright 2018 Google LLC
 */

#ifndef __CROS_VBOOT_H
#define __CROS_VBOOT_H

#include <cros/cros_ofnode.h>
#include <vb2_api.h>
#include <vboot_api.h>

/* Length of format ID */
#define ID_LEN		256U

/* Required alignment for struct vb2_context */
#define VBOOT_CONTEXT_ALIGN	16

/**
 * Information about each firmware type. We expect to have read-only,
 * read-write A, read-write B and recovery.
 *
 * @vblock: Pointer to the vblock if loaded - this is NULL except for RW-A and
 *	RW-B
 * @size: Size of firmware in bytes (this is the compressed size if the
 *	firmware is compressed)
 * @cache: Firmware data, if loaded
 * @uncomp_size: Uncompressed size of firmware. Same as @size if it is not
 *	compressed
 * @fw_entry: Pointer to the firmware entry in the fmap - there are three
 *	possible ones: RO, RW-A and RW-B. Note that RO includes recovery if
 *	this is a separate U-Boot from the RO U-Boot.
 * @entry: Pointer to the firmware entry that we plan to load and run.
 *	Normally this is U-Boot, but with EFS it is SPL, since it is the SPL
 *	that is signed by the signer, verified by vboot and jumped to by
 *	RO U-Boot.
 */
struct vboot_fw_info {
	void *vblock;
	ulong size;
	void *cache;
	size_t uncomp_size;
	struct fmap_section *fw_entry;
	struct fmap_entry *entry;
};

/**
 * struct vboot_blob - Vboot information in the bloblist
 *
 * This is persistent through the stages of vboot through TPL, SPL, etc.
 *
 * @ctx: vboot context
 * @spl_entry: used for the verstage to return the location of the selected
 *	SPL slot
 * @u_boot_entry: used for the verstage to return the location of the selected
 *	U-Boot slot
 */
struct vboot_blob {
	struct vb2_context ctx __aligned(VBOOT_CONTEXT_ALIGN);
	struct fmap_entry spl_entry;
	struct fmap_entry u_boot_entry;
};

/**
 * struct vboot_handoff - vboot information passed through the phases
 *
 * The vboot_handoff structure contains the data to be consumed by downstream
 * firmware after firmware selection has been completed. Namely it provides
 * vboot shared data as well as the flags from VbInit.
 *
 * @init_params: vboot parameters
 * @selected_firmware: This is the firmware selected to run (fw_slot)
 * @shared_data: Shared data used by vboot
 */
struct vboot_handoff {
	VbInitParams init_params;
	u32 selected_firmware;
	char shared_data[VB_SHARED_DATA_MIN_SIZE];
} __packed;

/**
 * Main verified boot data structure
 *
 * @valid: false if this structure is not yet set up, true if it is
 * @blob: Persistent blob in the bloblist
 * @ctx: vboot2 API context
 * @nvdata_dev: Device to use to access non-volatile data
 * @cros_ec: Chromium OS EC, or NULL if none
 * @gbb_flags: Copy of the flags from the Google Binary Block (GBB)
 * @tpm: TPM device
 * @video: Video device
 * @console: Video console (text device)
 * @panel: Display panel (can be NULL if there is none)
 * @config: Config node containing general configuation info
 *
 * @deactivate_tpm: Deactivate the TPM on startup
 * @disable_dev_on_rec: Disable developer mode if going into recovery
 * @ec-efs: EC uses early firmware selection
 * @ec_slow_update: Show a warning screen when updating the EC
 * @ec_software_sync: Platform supports EC software sync
 * @has_rec_mode_mrc: Recovery mode has a memory-reference-code (MRC) area
 * @meminit_in_ro: Memory init happens in read-only code
 * @oprom_matters: An option ROM is needed to init the display
 * @physical_dev_switch: Developer mode has a physical switch (i.e. not in TPM)
 * @physical_rec_switch: Recovery mode has a physical switch (i.e. not in TPM)
 * @resume_path_same_as_boot: Resume path boots through the reset vector
 *
 * @detachable_ui: Use the keyboard-less UI
 * @disable_memwipe: Disable memory wiping on this platform
 * @disable_lid_shutdown_during_update: Ignore LID closed during auxfw update
 * @disable_power_button_during_update: Disable the power button during an aux
 *	firmware update
 * @usb_is_enumerated: true if USB ports have been enumerated already
 * @work_buffer_size: Size of vboot2 work buffer
 *
 * @handoff: Vboot handoff info
 * @fmap: Firmare map, parsed from the binman information
 * @fwstore: Firmware storage device
 * @vboot_out_flags: Output flags provided by vboot
 * @kparams: Kernel params passed to Vboot library
 * @cparams: Common params passed to Vboot library
 * @vb2_return_code: Vboot library error, if any
 * @fw_size: Size of firmware image in bytes - this starts off as the number
 *	of bytes in the section containing the firmware, but may be smaller if
 *	the vblock indicates that not all of that data was signed.
 * @readonly_firmware_id: Firmware ID read from RO firmware
 * @firmware_id: Firmware ID of selected RO/RW firmware
 * @spl_image: SPL image provided to U-Boot so it knows what to boot into next
 */
struct vboot_info {
	bool valid;
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	struct udevice *nvdata_dev;
	struct udevice *cros_ec;
	u32 gbb_flags;
	struct udevice *tpm;
	struct udevice *video;
	struct udevice *console;
	struct udevice *panel;
	ofnode config;

	bool deactivate_tpm;
	bool disable_dev_on_rec;
	bool ec_efs;
	bool ec_slow_update;
	bool ec_software_sync;
	bool has_rec_mode_mrc;
	bool meminit_in_ro;
	bool oprom_matters;
	bool physical_dev_switch;
	bool physical_rec_switch;
	bool resume_path_same_as_boot;
#ifndef CONFIG_SPL_BUILD
	bool detachable_ui;
	bool disable_memwipe;
	bool disable_lid_shutdown_during_update;
	bool disable_power_button_during_update;
	bool usb_is_enumerated;
#endif
	int work_buffer_size;

	struct vboot_handoff *handoff;
	struct cros_fmap fmap;
	struct udevice *fwstore;
	u32 vboot_out_flags;
#ifndef CONFIG_SPL_BUILD
	VbSelectAndLoadKernelParams kparams;
	VbCommonParams cparams;
#endif
	enum vb2_return_code vb2_return_code;

	enum VbErrorPredefined_t vb_error;
	u32 fw_size;

	char readonly_firmware_id[ID_LEN];
	char firmware_id[ID_LEN];
	struct spl_image_info *spl_image;
};

/** enum secdata_t - field that can be read/written in secdata */
enum secdata_t {
	SECDATA_DEV_MODE,
	SECDATA_LAST_BOOT_DEV,

	SECDATA_COUNT,
	SECDATA_NONE
};

/**
 * ctx_to_vboot() - Get the vboot_info from a vb2 context
 *
 * @ctx: vb2 context
 * @return pointer to vboot_info record
 */
static inline struct vboot_info *ctx_to_vboot(struct vb2_context *ctx)
{
	return ctx->non_vboot_context;
}

/**
 * vboot_get_ctx() - Get the vb2 context from a vboot_info poitner
 *
 * @vboot: vboot_info pointer
 * @return pointer to vb2 context
 */
static inline struct vb2_context *vboot_get_ctx(struct vboot_info *vboot)
{
	return vboot->ctx;
}

/**
 * vboot_update_acpi() - Update ACPI data
 *
 * For x86 systems, this writes a basic level of information in binary to
 * the ACPI tables for use by the kernel.
 *
 * @vboot: Pointer to vboot structure
 * @return 0 if OK, -ve on error
 */
int vboot_update_acpi(struct vboot_info *vboot);

/**
 * vboot_get() - Get a pointer to the vboot structure
 *
 * @vboot: Pointer to vboot structure, if valid, else NULL
 */
struct vboot_info *vboot_get(void);

/**
 * vboot_alloc() - Allocate a vboot structure
 *
 * @vboot: returns pointer to allocated vboot structure on success
 * @return 0 if OK, -ENOMEM if out of memory
 */
int vboot_alloc(struct vboot_info **vbootp);

/**
 * vboot_get_alloc() - Get the reboot structure, allocating it if necessary
 *
 * @return pointer to vboot struct, or NULL if out of memory
 */
struct vboot_info *vboot_get_alloc(void);

/**
 * vboot_load_config() - Load configuration for vboot
 *
 * The configuration controls how vboot operates
 *
 * @vboot: Pointer to vboot structure to update
 * @return 0 if OK, -ve on error
 */
int vboot_load_config(struct vboot_info *vboot);

/**
 * vboot_platform_is_resuming() - check if we are resuming from suspend
 *
 * Determine if the platform is resuming from suspend
 *
 * @return 0 when not resuming, > 0 if resuming, < 0 on error.
 */
int vboot_platform_is_resuming(void);

/**
 * vboot_is_slot_a() - Check which slot is being used for boot
 *
 * @return true if slot A, false if slot B
 */
bool vboot_is_slot_a(struct vboot_info *vboot);

/**
 * vboot_slot_name() - Get the name of the slow being use for boot
 *
 * @return "A" if slot A, "B" if slot B
 */
const char *vboot_slot_name(struct vboot_info *vboot);

/**
 * vboot_set_selected_region() - Set the selected regions to boot from
 *
 * This records the flash regions containing SPL and U-Boot, which will be used
 * to locate these phases of the boot.
 *
 * @vboot: Pointer to vboot structure
 * @spl: Fwstore region to use for SPL
 * @u_boot: Fwstore region to use for U-Boot proper
 */
void vboot_set_selected_region(struct vboot_info *vboot,
			       const struct fmap_entry *spl,
			       const struct fmap_entry *u_boot);

/**
 * vboot_jump() - Jump to the given flash entry
 *
 * This is used to execute the code in a flashmap entry. Execution starts there
 * immediately. The data is loaded into RAM if needed
 *
 * @vboot: Pointer to vboot structure
 * @entry: Fwstore entry to jump to
 * @return -ve on error. On success this does not return
 */
int vboot_jump(struct vboot_info *vboot, struct fmap_entry *entry);

/**
 * vboot_wants_oprom() - Check if vboot needs an option ROM
 *
 * @return true if vboot needs an option ROM (as it intends to use the display
 *	and this platform uses OPROMs), false if not
 */
bool vboot_wants_oprom(struct vboot_info *vboot);

/**
 * vboot_fill_handoff() - Add the handoff information to vboot
 *
 * This writes out the handoff information so that the following phases know
 * what to do
 *
 * @vboot: Pointer to vboot structure
 */
int vboot_fill_handoff(struct vboot_info *vboot);

/**
 * vboot_get_gbb_flags() - Get the Google Binary Block (GBB) flags
 *
 * This can only be called after vboot_rw_init() is finished
 *
 * @vboot: Pointer to vboot structure
 * @return GBB flag value
 */
u32 vboot_get_gbb_flags(struct vboot_info *vboot);

/**
 * cros_tpm_extend_pcrs() - Extend TPM PCRs with the vboot digests
 *
 * Vboot generates digests for the boot mode and the hardware ID. This extends
 * TPM PCRs with these values
 *
 * @vboot: Pointer to vboot structure
 * @return 0 if OK, non-zero on error
 */
int cros_tpm_extend_pcrs(struct vboot_info *vboot);

/**
 * cros_tpm_factory_initialise() - Set up the TPM for the first time
 *
 * This sets up the TPM ready for use. It should be called if the TPM is found
 * to not be inited.
 *
 * @vboot: Pointer to vboot structure
 * @return 0 if OK, non-zero on error
 */
int cros_tpm_factory_initialise(struct vboot_info *vboot);

/**
 * cros_tpm_setup() - Set up the TPM ready for use
 *
 * @vboot: Pointer to vboot structure
 * @return 0 if OK, non-zero on error
 */
int cros_tpm_setup(struct vboot_info *vboot);

/**
 * vboot_dump_nvdata() - Dump the vboot non-volatile data
 *
 * This shows the NV data in human-readable form
 *
 * @nvdata: Pointer to context
 * @size: Size of NV data (typically EC_VBNV_BLOCK_SIZE)
 * @return 0 if it is valid, -ve error otherwise
 */
int vboot_dump_nvdata(const void *nvdata, int size);

/**
 * vboot_dump_nvdata() - Dump the vboot secure data
 *
 * This shows the context in human-readable form
 *
 * @nvdata: Pointer to context
 * @size: Size of NV context (typically sizeof(struct vb2_secdata))
 * @return 0 if it is valid, -ve error otherwise
 */
int vboot_secdata_dump(const void *secdata, int size);

/**
 * vboot_secdata_set() - Set a field in the secure data
 *
 * This is used to update a single field in the secure data, for testing and
 * development purpsoes
 *
 * @secdata: Pointer to vboot secure data
 * @size: Size of data in bytes
 * @field: Field to udpate
 * @val: Value to set
 * @return 0 if OK, -ve on error
 */
int vboot_secdata_set(void *secdata, int size, enum secdata_t field, int val);

/**
 * vboot_secdata_get() - Get a field from secure data
 *
 * This is read used read a single field in the secure data, for testing and
 * development purposes
 *
 * @secdata: Pointer to vboot secure data
 * @size: Size of data in bytes
 * @field: Field to read
 * @return value read, or -ve on error
 */
int vboot_secdata_get(const void *secdata, int size, enum secdata_t field);

#endif /* __CROS_VBOOT_H */
