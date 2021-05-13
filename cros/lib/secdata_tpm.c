// SPDX-License-Identifier:     BSD-3-Clause
/*
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 *
 * Taken from coreboot file secdata_tpm.c
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <log.h>
#include <tpm-common.h>
#include <tpm_api.h>
#include <cros/nvdata.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>

/*
 * Default data when setting up the TPM.
 *
 * This is derived from rollback_index.h of vboot_reference. see struct
 * RollbackSpaceKernel for details.
 */
static const u8 secdata_kernel[] = {
	0x02,
	0x4c, 0x57, 0x52, 0x47,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
	0xe8,
};

/*
 * Different sets of NVRAM space attributes apply to the "ro" spaces,
 * i.e. those which should not be possible to delete or modify once
 * the RO exits, and the rest of the NVRAM spaces.
 */
static const enum tpm_index_attrs v2_ro_space_attributes =
	TPMA_NV_PPWRITE |
	TPMA_NV_AUTHREAD |
	TPMA_NV_PPREAD |
	TPMA_NV_PLATFORMCREATE |
	TPMA_NV_WRITE_STCLEAR |
	TPMA_NV_POLICY_DELETE;

static const u32 v2_rw_space_attributes =
	TPMA_NV_PPWRITE |
	TPMA_NV_AUTHREAD |
	TPMA_NV_PPREAD |
	TPMA_NV_PLATFORMCREATE;

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
	0x44, 0x44, 0x79, 0x00, 0xCB, 0xB8, 0x3F, 0x5B, 0x15, 0x76, 0x56,
	0x50, 0xEF, 0x96, 0x98, 0x0A, 0x2B, 0x96, 0x6E, 0xA9, 0x09, 0x04,
	0x4A, 0x01, 0xB8, 0x5F, 0xA5, 0x4A, 0x96, 0xFC, 0x59, 0x84};

static const int v1_ro_space_attributes = TPM_NV_PER_GLOBALLOCK |
			TPM_NV_PER_PPWRITE;
static const int v1_rw_space_attributes = TPM_NV_PER_GLOBALLOCK |
			TPM_NV_PER_PPWRITE;
static const u8 v1_pcr0_unchanged_policy[0];

static uint32_t set_space(const char *name, uint32_t index, const void *data,
			  uint32_t length, const TPMA_NV nv_attributes,
			  const uint8_t *nv_policy, size_t nv_policy_size)
{
	uint32_t rv;

	rv = tlcl_define_space(index, length, nv_attributes, nv_policy,
			       nv_policy_size);
	if (rv == TPM_E_NV_DEFINED) {
		/*
		 * Continue with writing: it may be defined, but not written
		 * to. In that case a subsequent tlcl_read() would still return
		 * TPM_E_BADINDEX on TPM 2.0. The cases when some non-firmware
		 * space is defined while the firmware space is not there
		 * should be rare (interrupted initialization), so no big harm
		 * in writing once again even if it was written already.
		 */
		VBDEBUG("%s: %s space already exists\n", __func__, name);
		rv = TPM_SUCCESS;
	}

	if (rv != TPM_SUCCESS)
		return rv;

	return safe_write(index, data, length);
}

static uint32_t read_space_firmware(struct vb2_context *ctx)
{
	RETURN_ON_FAILURE(tlcl_read(FIRMWARE_NV_INDEX,
				    ctx->secdata_firmware,
				    VB2_SECDATA_FIRMWARE_SIZE));
	return TPM_SUCCESS;
}

uint32_t antirollback_read_space_kernel(struct vb2_context *ctx)
{
	if (!CONFIG(TPM2)) {
		/*
		 * Before reading the kernel space, verify its permissions. If
		 * the kernel space has the wrong permission, we give up. This
		 * will need to be fixed by the recovery kernel. We will have
		 * to worry about this because at any time (even with PP turned
		 * off) the TPM owner can remove and redefine a PP-protected
		 * space (but not write to it).
		 */
		uint32_t perms;

		RETURN_ON_FAILURE(tlcl_get_permissions(KERNEL_NV_INDEX,
						       &perms));
		if (perms != TPM_NV_PER_PPWRITE) {
			printk(BIOS_ERR,
			       "TPM: invalid secdata_kernel permissions\n");
			return TPM_E_CORRUPTED_STATE;
		}
	}

	uint8_t size = VB2_SECDATA_KERNEL_MIN_SIZE;

	RETURN_ON_FAILURE(tlcl_read(KERNEL_NV_INDEX, ctx->secdata_kernel,
				    size));

	if (vb2api_secdata_kernel_check(ctx, &size)
	    == VB2_ERROR_SECDATA_KERNEL_INCOMPLETE)
		/* Re-read. vboot will run the check and handle errors. */
		RETURN_ON_FAILURE(tlcl_read(KERNEL_NV_INDEX,
					    ctx->secdata_kernel, size));

	return TPM_SUCCESS;
}

/*
 * This is used to initialise the TPM space for recovery hash after defining
 * it. Since there is no data available to calculate hash at the point where TPM
 * space is defined, initialise it to all 0s.
 */
static const u8 rec_hash_data[REC_HASH_NV_SIZE] = {};

static int extend_pcr(struct vboot_info *vboot, int pcr,
		      enum vb2_pcr_digest which_digest)
{
	u8 buffer[VB2_PCR_DIGEST_RECOMMENDED_SIZE];
	u8 out[VB2_PCR_DIGEST_RECOMMENDED_SIZE];
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	u32 size = sizeof(buffer);
	int ret;

	ret = vb2api_get_pcr_digest(ctx, which_digest, buffer, &size);
	if (ret)
		return log_msg_retz("get", ret);
	if (size < TPM_PCR_MINIMUM_DIGEST_SIZE)
		return log_msg_retz("size", VB2_ERROR_UNKNOWN);

	ret = tpm_pcr_extend(vboot->tpm, pcr, buffer, out);
	if (ret)
		return log_msg_retz("extend", VB2_ERROR_UNKNOWN);

	return 0;
}

int cros_tpm_extend_pcrs(struct vboot_info *vboot)
{
	int ret;

	ret = extend_pcr(vboot, 0, BOOT_MODE_PCR);
	if (ret)
		return log_msg_retz("boot_mode", ret);
	ret = extend_pcr(vboot, 1, HWID_DIGEST_PCR);
	if (ret)
		return log_msg_retz("hwid", ret);

	return 0;
}

static uint32_t set_firmware_space(const void *firmware_blob)
{
	return set_space("firmware", FIRMWARE_NV_INDEX, firmware_blob,
			 VB2_SECDATA_FIRMWARE_SIZE, ro_space_attributes,
			 pcr0_allowed_policy, sizeof(pcr0_allowed_policy));
}

static uint32_t set_kernel_space(const void *kernel_blob)
{
	return set_space("kernel", KERNEL_NV_INDEX, kernel_blob,
			 VB2_SECDATA_KERNEL_SIZE, rw_space_attributes, NULL, 0);
}

static uint32_t set_mrc_hash_space(uint32_t index, const uint8_t *data)
{
	if (index == MRC_REC_HASH_NV_INDEX) {
		return set_space("RO MRC Hash", index, data, HASH_NV_SIZE,
				 ro_space_attributes, pcr0_allowed_policy,
				 sizeof(pcr0_allowed_policy));
	} else {
		return set_space("RW MRC Hash", index, data, HASH_NV_SIZE,
				 rw_space_attributes, NULL, 0);
	}
}

static int setup_space(struct udevice *dev, enum cros_nvdata_type type,
		       uint attr, const void *nv_policy, uint nv_policy_size,
		       const void *data, uint size)
{
	int ret;

	ret = cros_nvdata_setup_walk(type, attr, size, nv_policy,
				     nv_policy_size);
	if (ret)
		return ret;
	ret = cros_nvdata_write_walk(type, data, size);
	if (ret)
		return ret;

	return 0;
}

static int setup_spaces(struct vboot_info *vboot)
{
	enum tpm_version version = tpm_get_version(vboot->tpm);
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	/*
	 * Of all NVRAM spaces defined by this function the firmware space
	 * must be defined last, because its existence is considered an
	 * indication that TPM factory initialisation was successfully
	 * completed.
	 */
	ret = setup_space(vboot->tpm, CROS_NV_SECDATAK, version == TPM_V1 ?
			  v1_rw_space_attributes : v2_rw_space_attributes,
			  NULL, 0, secdata_kernel, sizeof(secdata_kernel));
	if (ret)
		return ret;

	/*
	 * Define and set rec hash space, if available.  No need to
	 * create the RW hash space because we will definitely boot
	 * once in normal mode before shipping, meaning that the space
	 * will get created with correct permissions while still in
	 * our hands.
	 */
	if (vboot->has_rec_mode_mrc) {
		ret = setup_space(vboot->tpm, CROS_NV_REC_HASH,
				  version == TPM_V1 ? v1_ro_space_attributes :
				  v2_ro_space_attributes,
				  version == TPM_V1 ? v1_pcr0_unchanged_policy :
				  v2_pcr0_unchanged_policy,
				  version == TPM_V1 ?
				  sizeof(v1_pcr0_unchanged_policy) :
				  sizeof(v2_pcr0_unchanged_policy),
				  rec_hash_data, sizeof(rec_hash_data));
		if (ret)
			return ret;
	}
	vb2api_secdata_firmware_create(ctx);
	ret = setup_space(vboot->tpm, CROS_NV_SECDATAF, version == TPM_V1 ?
			  v1_rw_space_attributes : v2_rw_space_attributes,
			  NULL, 0, ctx->secdata_firmware,
			  VB2_SECDATA_FIRMWARE_SIZE);
	if (ret)
		return ret;

	return 0;
}

uint32_t antirollback_lock_space_firmware(void)
{
	return tlcl_lock_nv_write(FIRMWARE_NV_INDEX);
}

uint32_t antirollback_read_space_mrc_hash(uint32_t index, uint8_t *data, uint32_t size)
{
	if (size != HASH_NV_SIZE) {
		VBDEBUG("TPM: Incorrect buffer size for hash idx 0x%x. "
			"(Expected=0x%x Actual=0x%x).\n", index, HASH_NV_SIZE,
			size);
		return TPM_E_READ_FAILURE;
	}
	return read_space_mrc_hash(index, data);
}

uint32_t antirollback_write_space_mrc_hash(uint32_t index, const uint8_t *data, uint32_t size)
{
	uint8_t spc_data[HASH_NV_SIZE];
	uint32_t rv;

	if (size != HASH_NV_SIZE) {
		VBDEBUG("TPM: Incorrect buffer size for hash idx 0x%x. "
			"(Expected=0x%x Actual=0x%x).\n", index, HASH_NV_SIZE,
			size);
		return TPM_E_WRITE_FAILURE;
	}

	rv = read_space_mrc_hash(index, spc_data);
	if (rv == TPM_E_BADINDEX) {
		/*
		 * If space is not defined already for hash, define
		 * new space.
		 */
		VBDEBUG("TPM: Initializing hash space.\n");
		return set_mrc_hash_space(index, data);
	}

	if (rv != TPM_SUCCESS)
		return rv;

	return safe_write(index, data, size);
}

uint32_t antirollback_lock_space_mrc_hash(uint32_t index)
{
	return tlcl_lock_nv_write(index);
}

static u32 v2_factory_initialise_tpm(struct vboot_info *vboot)
{
	int ret;

	log_warning("Setting up TPM for first time from factory\n");
	ret = tpm_force_clear(vboot->tpm);
	if (ret)
		return ret;

	return setup_spaces(vboot);
}

static int v1_factory_initialise_tpm(struct vboot_info *vboot)
{
	struct tpm_permanent_flags pflags;
	int ret;

	vb2api_secdata_kernel_create_v0(ctx);

	ret = tpm1_get_permanent_flags(vboot->tpm, &pflags);
	if (ret != TPM_SUCCESS)
		return -EIO;

	/*
	 * TPM may come from the factory without physical presence finalised.
	 * Fix if necessary.
	 */
	log_debug("physical_presence_lifetime_lock=%d\n",
		  pflags.physical_presence_lifetime_lock);
	if (!pflags.physical_presence_lifetime_lock) {
		log_debug("Finalising physical presence\n");
		ret = tpm_finalise_physical_presence(vboot->tpm);
		if (ret != TPM_SUCCESS)
			return -EIO;
	}

	/*
	 * The TPM will not enforce the NV authorization restrictions until the
	 * execution of a TPM_NV_DefineSpace with the handle of
	 * TPM_NV_INDEX_LOCK.  Here we create that space if it doesn't already
	 * exist
	 */
	log_debug("nv_locked=%d\n", pflags.nv_locked);
	if (!pflags.nv_locked) {
		log_debug("Enabling NV locking\n");
		ret = tpm_nv_enable_locking(vboot->tpm);
		if (ret != TPM_SUCCESS)
			return -EIO;
	}

	/* Clear TPM owner, in case the TPM is already owned for some reason */
	log_debug("TPM: Clearing owner\n");
	ret = tpm_clear_and_reenable(vboot->tpm);
	if (ret != TPM_SUCCESS)
		return -EIO;

	return setup_spaces(vboot);
}

uint32_t antirollback_lock_space_firmware(void)
{
	return tlcl_set_global_lock();
}

#endif

/**
 * Perform one-time initialisation
 *
 * Create the NVRAM spaces, and set their initial values as needed.  Sets the
 * nvLocked bit and ensures the physical presence command is enabled and
 * locked.
 */
int factory_initialise_tpm(struct vboot_info *vboot)
{
	enum tpm_version version = tpm_get_version(vboot->tpm);
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	/*
	 * Set initial values of secdata_firmware space.
	 * kernel space is created in _factory_initialise_tpm().
	 */
	vb2api_secdata_firmware_create(ctx);

	log_debug("TPM: factory initialisation\n");

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
		return -EIO;

	ret = -ENOSYS;
	switch (version) {
	case TPM_V1:
		if (IS_ENABLED(CONFIG_TPM_V1))
			ret = v1_factory_initialise_tpm(vboot);
		break;
	case TPM_V2:
		if (IS_ENABLED(CONFIG_TPM_V2))
			ret = v2_factory_initialise_tpm(vboot);
		break;
	if (ret)
		return log_msg_ret("init", ret);
	}

	/*
	 * ..._factory_initialize_tpm() writes initial secdata values to the TPM
	 * immediately, so let vboot know that it's up to date now
	 */
	ctx->flags &= ~(VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED |
			VB2_CONTEXT_SECDATA_KERNEL_CHANGED);

	log_debug("TPM: factory initialisation successful\n");

	return 0;
}

static u32 tpm_get_flags(struct udevice *dev, bool *disablep,
			 bool *deactivatedp, bool *nvlockedp)
{
	struct tpm_permanent_flags pflags;
	u32 ret = tpm1_get_permanent_flags(dev, &pflags);

	if (ret == TPM_SUCCESS) {
		if (disablep)
			*disablep = pflags.disable;
		if (deactivatedp)
			*deactivatedp = pflags.deactivated;
		if (nvlockedp)
			*nvlockedp = pflags.nv_locked;
		log_debug("TPM: flags disable=%d, deactivated=%d, nv_locked=%d\n",
			  pflags.disable, pflags.deactivated, pflags.nv_locked);
	}
	return ret;
}

static u32 tpm1_invoke_state_machine(struct vboot_info *vboot,
				     struct udevice *dev)
{
	bool disable;
	bool deactivated;
	u32 ret = TPM_SUCCESS;

	/* Check that the TPM is enabled and activated */
	ret = tpm_get_flags(dev, &disable, &deactivated, NULL);
	if (ret != TPM_SUCCESS) {
		log_err("TPM: Can't read capabilities\n");
		return ret;
	}

	if (!!deactivated != vboot->deactivate_tpm) {
		log_info("TPM: Unexpected TPM deactivated state; toggling..\n");
		ret = tpm_physical_set_deactivated(dev, !deactivated);
		if (ret != TPM_SUCCESS) {
			log_err("TPM: Can't toggle deactivated state\n");
			return ret;
		}

		deactivated = !deactivated;
		ret = TPM_E_MUST_REBOOT;
	}

	if (disable && !deactivated) {
		log_info("TPM: disabled (%d). Enabling..\n", disable);

		ret = tpm_physical_enable(dev);
		if (ret != TPM_SUCCESS) {
			log_err("TPM: Can't set enabled state\n");
			return ret;
		}

		log_info("TPM: Must reboot to re-enable\n");
		ret = TPM_E_MUST_REBOOT;
	}

	return ret;
}

/*
 * This starts the TPM and establishes the root of trust for the
 * anti-rollback mechanism.  This can fail for three reasons.  1 A bug. 2 a
 * TPM hardware failure. 3 An unexpected TPM state due to some attack.  In
 * general we cannot easily distinguish the kind of failure, so our strategy is
 * to reboot in recovery mode in all cases.  The recovery mode calls this code
 * again, which executes (almost) the same sequence of operations.  There is a
 * good chance that, if recovery mode was entered because of a TPM failure, the
 * failure will repeat itself.  (In general this is impossible to guarantee
 * because we have no way of creating the exact TPM initial state at the
 * previous boot.)  In recovery mode, we ignore the failure and continue, thus
 * giving the recovery kernel a chance to fix things (that's why we don't set
 * bGlobalLock).  The choice is between a known-insecure device and a
 * bricked device.
 *
 * As a side note, observe that we go through considerable hoops to avoid using
 * the STCLEAR permissions for the index spaces.  We do this to avoid writing
 * to the TPM flashram at every reboot or wake-up, because of concerns about
 * the durability of the NVRAM.
 */
static u32 do_setup(struct vboot_info *vboot, bool s3flag)
{
	u32 ret;

	log(UCLASS_TPM, LOGL_DEBUG, "Setting up TPM (s3=%d):\n", s3flag);
	ret = tpm_open(vboot->tpm);
	if (ret != TPM_SUCCESS) {
		log_err("TPM: Can't initialise\n");
		goto out;
	}

	/* Handle special init for S3 resume path */
	if (s3flag) {
		ret = tpm_resume(vboot->tpm);
		if (ret == TPM_INVALID_POSTINIT)
			log_info("TPM: Already initialised\n");

		return TPM_SUCCESS;
	}

	log(UCLASS_TPM, LOGL_DEBUG, "TPM startup:\n");
	ret = tpm_startup(vboot->tpm, TPM_ST_CLEAR);
	if (ret != TPM_SUCCESS) {
		log_err("TPM: Can't run startup command\n");
		goto out;
	}

	log(UCLASS_TPM, LOGL_DEBUG, "TPM presence:\n");
	ret = tpm_tsc_physical_presence(vboot->tpm,
					TPM_PHYSICAL_PRESENCE_PRESENT);
	if (ret != TPM_SUCCESS) {
		/*
		 * It is possible that the TPM was delivered with the physical
		 * presence command disabled.  This tries enabling it, then
		 * tries asserting PP again.
		 */
		ret = tpm_tsc_physical_presence(vboot->tpm,
					TPM_PHYSICAL_PRESENCE_CMD_ENABLE);
		if (ret != TPM_SUCCESS) {
			log_err("Can't enable physical presence command\n");
			goto out;
		}

		ret = tpm_tsc_physical_presence(vboot->tpm,
				TPM_PHYSICAL_PRESENCE_PRESENT);
		if (ret != TPM_SUCCESS) {
			log_err("Can't assert physical presence\n");
			goto out;
		}
	}

	if (tpm_get_version(vboot->tpm) == TPM_V1) {
		if (!IS_ENABLED(CONFIG_TPM_V1))
			return log_msg_ret("tpm_v1", -ENOSYS);
		ret = tpm1_invoke_state_machine(vboot, vboot->tpm);
		if (ret != TPM_SUCCESS)
			return ret;
	}

out:
	if (ret != TPM_SUCCESS)
		log_err("TPM: setup failed\n");
	else
		log_debug("TPM: setup succeeded\n");

	return ret;
}

int antirollback_read_space_firmware(struct vboot_info *vboot)
{
	uint32_t rv;

	/*
	 * Read secdata from TPM. initialise TPM if secdata not found. We don't
	 * check the return value here because vb2api_fw_phase1 will catch
	 * invalid secdata and tell us what to do (=reboot).
	 */
	ret = cros_nvdata_read_walk(CROS_NV_SECDATAF, ctx->secdata_firmware,
				    sizeof(ctx->secdata_firmware));
	if (ret == -ENOENT) {
		/* This seems the first time we've run. Initialise the TPM. */
		ret = factory_initialise_tpm(vboot);
		if (ret)
			return log_msg_ret("init", ret);
	} else if (ret) {
		log_err("TPM: Firmware space in a bad state; giving up\n");
		return log_msg_ret("secdata", ret);
	}
	vboot_secdata_dump(ctx->secdata_firmware, sizeof(ctx->secdata_firmware));
	log_debug("secdata:\n");
	log_buffer(LOGC_VBOOT, LOGL_DEBUG, 0, ctx->secdata_firmware, 1,
		   sizeof(ctx->secdata_firmware), 0);

	return TPM_SUCCESS;
}

uint32_t antirollback_write_space_firmware(struct vb2_context *ctx)
{
	if (CONFIG(CR50_IMMEDIATELY_COMMIT_FW_SECDATA))
		tlcl_cr50_enable_nvcommits();
	return safe_write(FIRMWARE_NV_INDEX, ctx->secdata_firmware,
			  VB2_SECDATA_FIRMWARE_SIZE);
}

uint32_t antirollback_write_space_kernel(struct vb2_context *ctx)
{
	/* Learn the expected size. */
	uint8_t size = VB2_SECDATA_KERNEL_MIN_SIZE;
	vb2api_secdata_kernel_check(ctx, &size);

	/*
	 * Ensure that the TPM actually commits our changes to NVMEN in case
	 * there is a power loss or other unexpected event. The AP does not
	 * write to the TPM during normal boot flow; it only writes during
	 * recovery, software sync, or other special boot flows. When the AP
	 * wants to write, it is imporant to actually commit changes.
	 */
	if (CONFIG(CR50_IMMEDIATELY_COMMIT_FW_SECDATA))
		tlcl_cr50_enable_nvcommits();

	return safe_write(KERNEL_NV_INDEX, ctx->secdata_kernel, size);
}

vb2_error_t vb2ex_tpm_clear_owner(struct vb2_context *ctx)
{
	struct vboot_info *vboot = vboot_get();
	u32 rv;

	log_info("Clearing TPM owner\n");
	rv = tpm_clear_and_reenable(vboot->tpm);
	if (rv)
		return VB2_ERROR_EX_TPM_CLEAR_OWNER;

	return VB2_SUCCESS;
}

int vboot_setup_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	ret = do_setup(vboot, ctx->flags & VB2_CONTEXT_S3_RESUME);
	if (ret == TPM_E_MUST_REBOOT)
		ctx->flags |= VB2_CONTEXT_SECDATA_WANTS_REBOOT;

	return ret;
}
