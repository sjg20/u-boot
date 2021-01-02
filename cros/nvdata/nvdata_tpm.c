// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_CROS_NVDATA

#include <common.h>
#include <dm.h>
#include <log.h>
#include <tpm_api.h>
#include <cros/nvdata.h>

/**
 * get_index() - Get the index corresponding to a category
 *
 * Convert an index into a TPM index
 *
 * @index: Index to convert
 * @return Corresponding TPM index, or -1 if none
 */
static int get_index(enum cros_nvdata_type type)
{
	switch (type) {
	case CROS_NV_SECDATA:
		return FIRMWARE_NV_INDEX;
	case CROS_NV_SECDATAK:
		return KERNEL_NV_INDEX;
	case CROS_NV_REC_HASH:
		return REC_HASH_NV_INDEX;
	default:
		/* We cannot handle these */
		break;
	}
	log_debug("Unsupported type %d\n", type);

	return -1;
}

/**
 * safe_write() - Writes a value safely to the TPM
 *
 * This checks for write errors due to hitting the 64-write limit and clears
 * the TPM when that happens.  This can only happen when the TPM is unowned,
 * so it is OK to clear it (and we really have no choice). This is not expected
 * to happen frequently, but it could happen.
 *
 * @tpm: TPM device
 * @index: NV index to write to
 * @data: Data to write
 * @length: Length of data
 */
static u32 safe_write(struct udevice *tpm, u32 index, const void *data,
		      u32 length)
{
	u32 ret = tpm_nv_write_value(tpm, index, data, length);

	if (ret == TPM_MAXNVWRITES) {
		ret = tpm_clear_and_reenable(tpm);
		if (ret != TPM_SUCCESS) {
			log_err("Unable to clear and re-enable TPM\n");
			return ret;
		}
		ret = tpm_nv_write_value(tpm, index, data, length);
	}
	if (ret) {
		log_err("Failed to write secdata (err=%x)\n", ret);
		return -EIO;
	}

	return 0;
}

int tpm_secdata_read(struct udevice *dev, enum cros_nvdata_type type, u8 *data,
		     int size)
{
	struct udevice *tpm = dev_get_parent(dev);
	int index;
	int ret;

	index = get_index(type);
	if (index == -1)
		return -EINVAL;

	ret = tpm_nv_read_value(tpm, index, data, size);
	if (ret == TPM_BADINDEX) {
		return log_msg_ret("TPM has no secdata for index", -ENOENT);
	} else if (ret != TPM_SUCCESS) {
		log_err("Failed to read secdata (err=%x)\n", ret);
		return -EIO;
	}

	return 0;
}

static int tpm_secdata_write(struct udevice *dev, enum cros_nvdata_type type,
			     const u8 *data, int size)
{
	struct udevice *tpm = dev_get_parent(dev);
	int index;
	int ret;

	index = get_index(type);
	if (index == -1)
		return -EINVAL;

	ret = safe_write(tpm, index, data, size);
	if (ret != TPM_SUCCESS) {
		log_err("Failed to write secdata (err=%x)\n", ret);
		return -EIO;
	}

	return 0;
}

/**
 * Similarly to safe_write(), this ensures we don't fail a DefineSpace because
 * we hit the TPM write limit. This is even less likely to happen than with
 * writes because we only define spaces once at initialisation, but we'd
 * rather be paranoid about this.
 */
static u32 safe_define_space(struct udevice *tpm, u32 index, u32 perm, u32 size)
{
	u32 result;

	result = tpm1_nv_define_space(tpm, index, perm, size);
	if (result == TPM_MAXNVWRITES) {
		result = tpm_clear_and_reenable(tpm);
		if (result != TPM_SUCCESS)
			return result;
		return tpm1_nv_define_space(tpm, index, perm, size);
	} else {
		return result;
	}
}

static uint32_t set_space(struct udevice *tpm, uint index, uint attr, uint size,
			  const u8 *nv_policy, size_t nv_policy_size)
{
	uint32_t rv;

	rv = tpm2_nv_define_space(tpm, index, size, attr, nv_policy,
				  nv_policy_size);
	if (rv == TPM2_RC_NV_DEFINED) {
		/*
		 * Continue with writing: it may be defined, but not written
		 * to. In that case a subsequent tlcl_read() would still return
		 * TPM_E_BADINDEX on TPM 2.0. The cases when some non-firmware
		 * space is defined while the firmware space is not there
		 * should be rare (interrupted initialization), so no big harm
		 * in writing once again even if it was written already.
		 */
		log_debug("%#x space already exists\n", index);
		rv = TPM_SUCCESS;
	}

	if (rv != TPM_SUCCESS)
		return rv;

	return 0;
}

static int tpm_secdata_setup(struct udevice *dev, enum cros_nvdata_type type,
			     uint attr, uint size, const u8 *nv_policy,
			     int nv_policy_size)
{
	struct udevice *tpm = dev_get_parent(dev);
	enum tpm_version version = tpm_get_version(tpm);
	int ret;
	int index;

	index = get_index(type);
	if (index == -1)
		return -EINVAL;

	if (IS_ENABLED(CONFIG_TPM_V1) && version == TPM_V1)
		ret = safe_define_space(tpm, index, attr, size);
	else if (IS_ENABLED(CONFIG_TPM_V2) && version == TPM_V2)
		ret = set_space(dev, index, attr, size, nv_policy,
				nv_policy_size);
	else
		return log_msg_ret("no tpm", -ENOENT);
	if (ret != TPM_SUCCESS) {
		log_err("Failed to setup secdata (err=%x)\n", ret);
		return -EIO;
	}

	return 0;
}

static int tpm_secdata_lock(struct udevice *dev, enum cros_nvdata_type type)
{
	struct udevice *tpm = dev_get_parent(dev);
	enum tpm_version version = tpm_get_version(tpm);
	int index;

	index = get_index(type);
	if (index == -1)
		return -EINVAL;

	if (IS_ENABLED(CONFIG_TPM_V1) && version == TPM_V1) {
		/*
		 * We only have a global lock. Lock it when the firmware space
		 * is requested, and do nothing otherwise. This ensures that the
		 * lock is always set.
		 */
		if (type == CROS_NV_SECDATA)
			return tpm_set_global_lock(tpm);
	} else if (IS_ENABLED(CONFIG_TPM_V2) && version == TPM_V2) {
		if (type != CROS_NV_SECDATA)
			return log_msg_ret("not secdata", -ENOTSUPP);
		return log_retz(tpm2_write_lock(tpm, index));
	} else {
		return log_msg_ret("no tpm", -ENOENT);
	}

	return 0;
}

static const struct cros_nvdata_ops tpm_secdata_ops = {
	.read	= tpm_secdata_read,
	.write	= tpm_secdata_write,
	.setup	= tpm_secdata_setup,
	.lock	= tpm_secdata_lock,
};

static const struct udevice_id tpm_secdata_ids[] = {
	{ .compatible = "google,tpm-secdata" },
	{ }
};

U_BOOT_DRIVER(google_tpm_secdata) = {
	.name		= "google_tpm_secdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= tpm_secdata_ids,
	.ops		= &tpm_secdata_ops,
};
