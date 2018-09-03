// SPDX-License-Identifier: GPL-2.0
/*
 * TPM setup and PCR-extend functions
 *
 * Copyright 2021 Google LLC
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <tpm_api.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>

#define TPM_PCR_BOOT_MODE	"VBOOT: boot mode"
#define TPM_PCR_GBB_HWID_NAME	"VBOOT: GBB HWID"

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
 *
 * Note: This function is called tpm_setup() in coreboot and is in tspi.c
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

int vboot_setup_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	ret = do_setup(vboot, ctx->flags & VB2_CONTEXT_S3_RESUME);
	if (ret == TPM_E_MUST_REBOOT)
		ctx->flags |= VB2_CONTEXT_SECDATA_WANTS_REBOOT;

	return ret;
}

vb2_error_t vboot_extend_pcr(struct vboot_info *vboot, int pcr,
			     enum vb2_pcr_digest which_digest)
{
	uint8_t buffer[VB2_PCR_DIGEST_RECOMMENDED_SIZE];
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	u32 size = sizeof(buffer);
	vb2_error_t rv;
	int ret;

	rv = vb2api_get_pcr_digest(ctx, which_digest, buffer, &size);
	if (rv != VB2_SUCCESS)
		return log_msg_retz("digest", rv);
	if (size < TPM_PCR_MINIMUM_DIGEST_SIZE)
		return log_msg_retz("size", VB2_ERROR_UNKNOWN);

	/*
	 * On TPM 1.2, all PCRs are intended for use with SHA1. We truncate our
	 * SHA256 HWID hash to 20 bytes to make it fit. On TPM 2.0, we always
	 * want to use the SHA256 banks, even for the boot mode which is
	 * technically a SHA1 value for historical reasons. vboot has already
	 * zero-extended the buffer to 32 bytes for us, so we just take it like
	 * that and pretend it's a SHA256. In practice, this means we never care
	 * about the (*size) value returned from vboot (which indicates how many
	 * significant bytes vboot wrote, although it always extends zeroes up
	 * to the end of the buffer), we always use a hardcoded size instead.
	 */
	_Static_assert(sizeof(buffer) >= VB2_SHA256_DIGEST_SIZE,
		       "Buffer needs to be able to fit at least a SHA256");
	enum vb2_hash_algorithm algo = tpm_is_v1(vboot->tpm) ? VB2_HASH_SHA1 :
		VB2_HASH_SHA256;

	switch (which_digest) {
	/* SHA1 of (devmode|recmode|keyblock) bits */
	case BOOT_MODE_PCR:
		ret = tpm_pcr_extend(vboot->tpm, pcr, buffer,
				     vb2_digest_size(algo), buffer,
				     TPM_PCR_BOOT_MODE);
		if (ret)
			return log_msg_retz("boot", ret);
		break;
	 /* SHA256 of HWID */
	case HWID_DIGEST_PCR:
		ret = tpm_pcr_extend(vboot->tpm, pcr, buffer,
				     vb2_digest_size(algo), buffer,
				     TPM_PCR_GBB_HWID_NAME);
		if (ret)
			return log_msg_retz("hwid", ret);
		break;
	default:
		return log_msg_retz("none", VB2_ERROR_UNKNOWN);
	}

	return 0;
}

int vboot_extend_pcrs(struct vboot_info *vboot)
{
	int ret;

	ret = vboot_extend_pcr(vboot, 0, BOOT_MODE_PCR);
	if (ret)
		return log_msg_ret("boot", -EIO);

	ret = vboot_extend_pcr(vboot, 1, HWID_DIGEST_PCR);
	if (ret)
		return log_msg_ret("hwid", -EIO);

	return 0;
}
