// SPDX-License-Identifier: BSD-3-Clause
/*
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <log.h>
#include <tpm-common.h>
#include <tpm_api.h>
#include <cros/antirollback.h>
#include <cros/cros_common.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>

static int read_space_firmware(struct vb2_context *ctx)
{
	int ret;

	ret = cros_nvdata_read_walk(CROS_NV_SECDATAF, ctx->secdata_firmware,
				    VB2_SECDATA_FIRMWARE_SIZE);
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

int antirollback_read_space_kernel(const struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	u8 size;
	int ret;

	if (tpm_is_v1(vboot->tpm)) {
		/*
		 * Before reading the kernel space, verify its permissions. If
		 * the kernel space has the wrong permission, we give up. This
		 * will need to be fixed by the recovery kernel. We will have
		 * to worry about this because at any time (even with PP turned
		 * off) the TPM owner can remove and redefine a PP-protected
		 * space (but not write to it).
		 */
		u32 perms;

		ret = tpm_get_permissions(vboot->tpm, KERNEL_NV_INDEX, &perms);
		if (ret)
			return log_msg_retz("gperm", ret);

		if (perms != TPM_NV_PER_PPWRITE) {
			log_err("TPM: invalid secdata_kernel permissions %x\n",
				perms);
			return log_msg_ret("perm", -EBADFD);
		}
	}

	size = VB2_SECDATA_KERNEL_MIN_SIZE;
	ret = cros_nvdata_read_walk(CROS_NV_SECDATAK, ctx->secdata_kernel,
				    size);
	if (ret)
		return log_msg_ret("read1", ret);

	if (vb2api_secdata_kernel_check(ctx, &size)
	    == VB2_ERROR_SECDATA_KERNEL_INCOMPLETE) {
		/* Re-read. vboot will run the check and handle errors */
		ret = cros_nvdata_read_walk(CROS_NV_SECDATAK,
					    ctx->secdata_kernel, size);
		if (ret)
			return log_msg_ret("read1", ret);
	}

	return 0;
}

static int read_space_mrc_hash(enum cros_nvdata_type type, u8 *data)
{
	int ret;

	ret = cros_nvdata_read_walk(type, data, HASH_NV_SIZE);
	if (ret)
		return log_msg_ret("read1", ret);

	return 0;
}

/*
 * This is used to initialize the TPM space for recovery hash after defining
 * it. Since there is no data available to calculate hash at the point where TPM
 * space is defined, initialize it to all 0s.
 */
static const uint8_t mrc_hash_data[HASH_NV_SIZE] = { };

/*
 * Different sets of NVRAM space attributes apply to the "ro" spaces,
 * i.e. those which should not be possible to delete or modify once
 * the RO exits, and the rest of the NVRAM spaces.
 */
static const enum tpm_index_attrs ro_space_attributes =
	TPMA_NV_PPWRITE |
	TPMA_NV_AUTHREAD |
	TPMA_NV_PPREAD |
	TPMA_NV_PLATFORMCREATE |
	TPMA_NV_WRITE_STCLEAR |
	TPMA_NV_POLICY_DELETE;

static const u32 rw_space_attributes =
	TPMA_NV_PPWRITE |
	TPMA_NV_AUTHREAD |
	TPMA_NV_PPREAD |
	TPMA_NV_PLATFORMCREATE;

static const u32 fwmp_attr =
	TPMA_NV_PLATFORMCREATE |
	TPMA_NV_OWNERWRITE |
	TPMA_NV_AUTHREAD |
	TPMA_NV_PPREAD |
	TPMA_NV_PPWRITE;

/*
 * This policy digest was obtained using TPM2_PolicyOR on 3 digests
 * corresponding to a sequence of
 *   -) TPM2_PolicyCommandCode(TPM_CC_NV_UndefineSpaceSpecial),
 *   -) TPM2_PolicyPCR(PCR0, <extended_value>).
 * where <extended value> is
 *   1) all zeros = initial, unextended state:
 *      - Value to extend to initial PCR0:
 *        <none>
 *      - Resulting PCR0:
 *        0000000000000000000000000000000000000000000000000000000000000000
 *      - Policy digest for PolicyCommandCode + PolicyPCR:
 *        4B44FC4192DB5AD7167E0135708FD374890A06BFB56317DF01F24F2226542A3F
 *   2) result of extending (SHA1(0x00|0x01|0x00) | 00s to SHA256 size)
 *      - Value to extend to initial PCR0:
 *        62571891215b4efc1ceab744ce59dd0b66ea6f73000000000000000000000000
 *      - Resulting PCR0:
 *        9F9EA866D3F34FE3A3112AE9CB1FBABC6FFE8CD261D42493BC6842A9E4F93B3D
 *      - Policy digest for PolicyCommandCode + PolicyPCR:
 *        CB5C8014E27A5F7586AAE42DB4F9776A977BCBC952CA61E33609DA2B2C329418
 *   3) result of extending (SHA1(0x01|0x01|0x00) | 00s to SHA256 size)
 *      - Value to extend to initial PCR0:
 *        47ec8d98366433dc002e7721c9e37d5067547937000000000000000000000000
 *      - Resulting PCR0:
 *        2A7580E5DA289546F4D2E0509CC6DE155EA131818954D36D49E027FD42B8C8F8
 *      - Policy digest for PolicyCommandCode + PolicyPCR:
 *        E6EF4F0296AC3EF0F53906480985B1BE8058E0E517E5F74A5B8A415EFE339D87
 * Values #2 and #3 correspond to two forms of recovery mode as extended by
 * vb2api_get_pcr_digest().
 * As a result, the digest allows deleting the space with UndefineSpaceSpecial
 * at early RO stages (before extending PCR0) or from recovery mode.
 */
static const uint8_t pcr0_allowed_policy[] = {
	0x44, 0x44, 0x79, 0x00, 0xcb, 0xb8, 0x3f, 0x5b, 0x15, 0x76, 0x56,
	0x50, 0xef, 0x96, 0x98, 0x0a, 0x2b, 0x96, 0x6e, 0xa9, 0x09, 0x04,
	0x4a, 0x01, 0xb8, 0x5f, 0xa5, 0x4a, 0x96, 0xfc, 0x59, 0x84};

static int safe_write(enum cros_nvdata_type type, const void *data, u32 length)
{
	int ret;

	/* the nvdata_tpm driver handles retrying if needed */
	ret = cros_nvdata_write_walk(type, data, length);
	if (ret)
		return log_msg_ret("fwrite", -EIO);

	return 0;
}

static int setup_space(const char *name, enum cros_nvdata_type type,
		       const void *data, u32 length, const uint nv_attributes,
		       const uint8_t *nv_policy, size_t nv_policy_size)
{
	int ret;

	/* the nvdata_tpm driver handles retrying if needed */
	ret = cros_nvdata_setup_walk(type, nv_attributes, length, nv_policy,
				     nv_policy_size);
	if (ret)
		return log_msg_ret("setup", ret);

	log_buffer(UCLASS_TPM, LOGL_INFO, 0, data, 1, length, 0);

	ret = safe_write(type, data, length);
	if (ret)
		return log_msg_ret("write", ret);

	return 0;
}

static u32 setup_firmware_space(struct vb2_context *ctx)
{
	uint firmware_space_size = vb2api_secdata_firmware_create(ctx);

	return setup_space("firmware", CROS_NV_SECDATAF, ctx->secdata_firmware,
			   firmware_space_size, ro_space_attributes,
			   pcr0_allowed_policy, sizeof(pcr0_allowed_policy));
}

static uint32_t setup_fwmp_space(struct vb2_context *ctx)
{
	uint32_t fwmp_space_size = vb2api_secdata_fwmp_create(ctx);

	return setup_space("FWMP", CROS_NV_FWMP, ctx->secdata_fwmp,
			   fwmp_space_size, fwmp_attr, NULL, 0);
}

static u32 setup_kernel_space(struct vb2_context *ctx)
{
	uint kernel_space_size = vb2api_secdata_kernel_create(ctx);

	return setup_space("kernel", CROS_NV_SECDATAK, ctx->secdata_kernel,
			   kernel_space_size, rw_space_attributes, NULL,
			   0);
}

static u32 set_mrc_hash_space(enum cros_nvdata_type type, const uint8_t *data)
{
	if (type == CROS_NV_MRC_REC_HASH) {
		return setup_space("RO MRC Hash", type, data, HASH_NV_SIZE,
				   ro_space_attributes, pcr0_allowed_policy,
				   sizeof(pcr0_allowed_policy));
	} else {
		return setup_space("RW MRC Hash", type, data, HASH_NV_SIZE,
				   rw_space_attributes, NULL, 0);
	}
}

static int v2_factory_initialize_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	log_notice("Init TPM v2\n");
	ret = tpm_force_clear(vboot->tpm);
	if (ret != TPM_SUCCESS)
		return log_msg_ret("clear", -EIO);

	/*
	 * Of all NVRAM spaces defined by this function the firmware space
	 * must be defined last, because its existence is considered an
	 * indication that TPM factory initialization was successfully
	 * completed.
	 */
	ret = setup_kernel_space(ctx);
	if (ret)
		return log_msg_ret("kern", ret);

	/*
	 * Define and set rec hash space, if available.  No need to
	 * create the RW hash space because we will definitely boot
	 * once in normal mode before shipping, meaning that the space
	 * will get created with correct permissions while still in
	 * our hands.
	 */
	if (vboot->has_rec_mode_mrc) {
		ret = set_mrc_hash_space(CROS_NV_MRC_REC_HASH, mrc_hash_data);
		if (ret)
			return log_msg_ret("rec", ret);
	}

	/* Define and write firmware management parameters space. */
	ret = setup_fwmp_space(ctx);
	if (ret)
		return log_msg_ret("fwmp", ret);

	ret = setup_firmware_space(ctx);
	if (ret)
		return log_msg_ret("fw", ret);
	log_warning("done\n");

	return TPM_SUCCESS;
}

int antirollback_lock_space_firmware(void)
{
	int ret;

	ret = cros_nvdata_lock_walk(CROS_NV_SECDATAF);
	if (ret)
		return log_msg_ret("lock", ret);

	return 0;
}

int antirollback_read_space_mrc_hash(enum cros_nvdata_type type, uint8_t *data,
				     u32 size)
{
	if (size != HASH_NV_SIZE) {
		log_debug("TPM: Incorrect buffer size for hash type 0x%x. "
			"(Expected=0x%x Actual=0x%x).\n", type, HASH_NV_SIZE,
			size);
		return TPM_E_READ_FAILURE;
	}

	return read_space_mrc_hash(type, data);
}

int antirollback_write_space_mrc_hash(enum cros_nvdata_type type,
				      const uint8_t *data, u32 size)
{
	uint8_t spc_data[HASH_NV_SIZE];
	int ret;

	if (size != HASH_NV_SIZE) {
		log_debug("TPM: Incorrect buffer size for hash type 0x%x. "
			"(Expected=0x%x Actual=0x%x).\n", type, HASH_NV_SIZE,
			size);
		return TPM_E_WRITE_FAILURE;
	}

	ret = read_space_mrc_hash(type, spc_data);
	if (ret == -ENOENT) {
		/*
		 * If space is not defined already for hash, define
		 * new space.
		 */
		log_debug("TPM: Initializing hash space.\n");
		return set_mrc_hash_space(type, data);
	}
	if (ret != TPM_SUCCESS)
		return ret;

	return safe_write(type, data, size);
}

int antirollback_lock_space_mrc_hash(enum cros_nvdata_type type)
{
	int ret;

	ret = cros_nvdata_lock_walk(CROS_NV_SECDATAF);
	if (ret != TPM_SUCCESS)
		return log_msg_ret("flags", ret);

	return 0;
}

static int v1_factory_initialize_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	struct tpm_permanent_flags pflags;
	int ret;

	log_notice("Init TPM v1.2\n");
	vb2api_secdata_kernel_create_v0(ctx);

	ret = tpm1_get_permanent_flags(vboot->tpm, &pflags);
	if (ret != TPM_SUCCESS)
		return log_msg_ret("flags", -EIO);

	/*
	 * TPM may come from the factory without physical presence finalized.
	 * Fix if necessary.
	 */
	log_debug("TPM: physical_presence_lifetime_lock=%d\n",
		 pflags.physical_presence_lifetime_lock);
	if (!pflags.physical_presence_lifetime_lock) {
		log_info("TPM: Finalizing physical presence\n");
		ret = tpm_finalise_physical_presence(vboot->tpm);
		if (ret != TPM_SUCCESS)
			return log_msg_ret("final", -EIO);
	}

	/*
	 * The TPM will not enforce the NV authorization restrictions until the
	 * execution of a TPM_NV_DefineSpace with the handle of
	 * TPM_NV_INDEX_LOCK.  Here we create that space if it doesn't already
	 * exist */
	log_debug("TPM: nv_locked=%d\n", pflags.nv_locked);
	if (!pflags.nv_locked) {
		log_debug("TPM: Enabling NV locking\n");
		ret = tpm_nv_enable_locking(vboot->tpm);
		if (ret != TPM_SUCCESS)
			return log_msg_ret("lock", -EIO);
	}

	/* Clear TPM owner, in case the TPM is already owned for some reason */
	log_debug("TPM: Clearing owner\n");
	ret = tpm_clear_and_reenable(vboot->tpm);
	if (ret)
		return log_msg_ret("enable", -EIO);

	/* Define and write secdata_kernel space */
	ret = cros_nvdata_setup_walk(CROS_NV_SECDATAK, TPM_NV_PER_PPWRITE,
				     VB2_SECDATA_KERNEL_SIZE_V02, NULL, 0);
	if (ret)
		return log_msg_ret("ksetup", -EIO);
	ret = cros_nvdata_write_walk(CROS_NV_SECDATAK, ctx->secdata_kernel,
				     VB2_SECDATA_KERNEL_SIZE_V02);
	if (ret)
		return log_msg_ret("kwrite", -EIO);

	/* Define and write secdata_firmware space */
	ret = cros_nvdata_setup_walk(CROS_NV_SECDATAF, TPM_NV_PER_GLOBALLOCK |
				     TPM_NV_PER_PPWRITE,
				     VB2_SECDATA_FIRMWARE_SIZE, NULL, 0);
	if (ret)
		return log_msg_ret("fsetup", -EIO);
	ret = cros_nvdata_write_walk(CROS_NV_SECDATAF, ctx->secdata_firmware,
					VB2_SECDATA_FIRMWARE_SIZE);
	if (ret)
		return log_msg_ret("fwrite", -EIO);
	log_warning("done\n");

	return TPM_SUCCESS;
}

/**
 * Perform one-time initializations.
 *
 * Create the NVRAM spaces, and set their initial values as needed.  Sets the
 * nvLocked bit and ensures the physical presence command is enabled and
 * locked.
 */
static int factory_initialize_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	/*
	 * Set initial values of secdata_firmware space.
	 * kernel space is created in _factory_initialize_tpm().
	 */
	vb2api_secdata_firmware_create(ctx);

	log_debug("TPM: factory initialization\n");

	/*
	 * Do a full test.  This only happens the first time the device is
	 * turned on in the factory, so performance is not an issue.  This is
	 * almost certainly not necessary, but it gives us more confidence
	 * about some code paths below that are difficult to
	 * test---specifically the ones that set lifetime flags, and are only
	 * executed once per physical TPM.
	 */
	ret = tpm_self_test_full(vboot->tpm);
	if (ret)
		return log_msg_ret("selftest", ret);

	ret = -ENOSYS;
	if (tpm_is_v1(vboot->tpm))
		ret = v1_factory_initialize_tpm(vboot);
	else if (tpm_is_v2(vboot->tpm))
		ret = v2_factory_initialize_tpm(vboot);
	if (ret)
		return log_msg_ret("init", ret);

	/*
	 * _factory_initialize_tpm() writes initial secdata values to TPM
	 * immediately, so let vboot know that it's up to date now
	 */
	ctx->flags &= ~(VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED |
			VB2_CONTEXT_SECDATA_KERNEL_CHANGED);

	log_debug("TPM: factory initialization successful\n");

	return TPM_SUCCESS;
}

int antirollback_read_space_firmware(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	/* Read the firmware space */
	ret = read_space_firmware(ctx);
	if (ret == -ENOENT) {
		/* This seems the first time we've run. Initialize the TPM */
		log_warning("TPM: Not initialized yet\n");
		ret = factory_initialize_tpm(vboot);
		if (ret) {
			log_err("TPM: Firmware space in a bad state; giving up\n");
			return ret;
		}
	} else if (ret) {
		return log_msg_ret("read", ret);
	}

	return 0;
}

int antirollback_write_space_firmware(const struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	if (vboot->cr50_commit_secdata) {
		ret = tpm2_cr50_enable_nvcommits(vboot->tpm);
		if (ret)
			log_warning("Failed to enable Cr50 NV commits\n");
	}

	ret = cros_nvdata_write_walk(CROS_NV_SECDATAF, ctx->secdata_firmware,
				     VB2_SECDATA_FIRMWARE_SIZE);
	if (ret)
		return log_msg_ret("write", ret);

	return 0;
}

int antirollback_write_space_kernel(const struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	uint8_t size = VB2_SECDATA_KERNEL_MIN_SIZE;
	int ret;

	/* Learn the expected size */
	vb2api_secdata_kernel_check(ctx, &size);

	/*
	 * Ensure that the TPM actually commits our changes to NVMEN in case
	 * there is a power loss or other unexpected event. The AP does not
	 * write to the TPM during normal boot flow; it only writes during
	 * recovery, software sync, or other special boot flows. When the AP
	 * wants to write, it is important to actually commit changes.
	 */
	if (vboot->cr50_commit_secdata) {
		ret = tpm2_cr50_enable_nvcommits(vboot->tpm);
		if (ret)
			log_warning("Failed to enable Cr50 NV commits\n");
	}

	ret = cros_nvdata_write_walk(CROS_NV_SECDATAK, ctx->secdata_kernel,
				     size);
	if (ret)
		return log_msg_ret("write", ret);

	return 0;
}
