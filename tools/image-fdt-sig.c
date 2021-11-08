// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google, LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include "mkimage.h"
#include <fdt_region.h>
#include <image.h>
#include <version.h>

struct fdt_priv {
};

static int fdt_setup_sig(struct image_sign_info *info, const char *keydir,
			 const char *keyfile, const char *keyname, void *blob,
			 const char *algo_name, const char *padding_name,
			 const char *require_keys, const char *engine_id)
{
	memset(info, '\0', sizeof(*info));
	info->keydir = keydir;
	info->keyfile = keyfile;
	info->keyname = keyname;
	info->fit = blob;
	info->name = strdup(algo_name);
	info->checksum = image_get_checksum_algo(algo_name);
	info->crypto = image_get_crypto_algo(algo_name);
	info->padding = image_get_padding_algo(padding_name);
	info->require_keys = require_keys;
	info->engine_id = engine_id;
	if (!info->checksum || !info->crypto) {
		printf("Unsupported signature algorithm (%s)\n", algo_name);
		return -1;
	}

	return 0;
}

static int h_exclude_nodes(void *priv, const void *fdt, int offset, int type,
			   const char *data, int size)
{
	if (type == FDT_IS_NODE) {
		if (!strcmp("/chosen", data) || !strcmp("/signatures", data))
			return 0;
	}

	return -1;
}

static int run_find_regions(const void *fdt,
		int (*include_func)(void *priv, const void *fdt, int offset,
				 int type, const char *data, int size),
		struct fdt_priv *disp, struct fdt_region *region,
		int max_regions, char *path, int path_len, int flags)
{
	struct fdt_region_state state;
	int count;
	int ret;

	count = 0;
	ret = fdt_first_region(fdt, include_func, disp,
			&region[count++], path, path_len,
			flags, &state);
	while (ret == 0) {
		ret = fdt_next_region(fdt, include_func, disp,
				count < max_regions ? &region[count] : NULL,
				path, path_len, flags, &state);
		if (!ret)
			count++;
	}
	if (ret && ret != -FDT_ERR_NOTFOUND)
		return ret;

	return count;
}

/**
 * fdt_get_regions() - Get the regions to sign
 *
 * This calculates a list of node to hash for this particular configuration,
 * then finds which regions of the devicetree they correspond to.
 *
 * @fit:	Pointer to the FIT format image header
 * @conf_noffset: Offset of configuration node to sign (child of
 *	/configurations node)
 * @sig_offset:	Offset of signature node containing info about how to sign it
 *	(child of 'signatures' node)
 * @regionp: Returns list of regions that need to be hashed (allocated; must be
 *	freed by the caller)
 * @region_count: Returns number of regions
 * @return 0 if OK, -ENOMEM if out of memory, -EIO if the regions to hash could
 * not be found, -EINVAL if no registers were found to hash
 */
static int fdt_get_regions(const void *blob, struct image_region **regionp,
			   int *region_countp)
{
	struct image_region *region;
	struct fdt_region fdt_regions[100];
	char path[200];
	int count;

	/* Get a list of regions to hash */
	count = run_find_regions(blob, h_exclude_nodes, NULL, fdt_regions,
				 ARRAY_SIZE(fdt_regions), path, sizeof(path),
				 FDT_REG_SUPERNODES);
	if (count < 0) {
		fprintf(stderr, "Failed to hash device tree\n");
		return -EIO;
	}
	if (count == 0) {
		fprintf(stderr, "No data to hash for device tree");
		return -EINVAL;
	}

	/* Build our list of data blocks */
	region = fit_region_make_list(blob, fdt_regions, count, NULL);
	if (!region) {
		fprintf(stderr, "Out of memory making region list'\n");
		return -ENOMEM;
	}

	*region_countp = count;
	*regionp = region;

	return 0;
}

/**
 * fdt_write_sig() - write the signature to an FDT
 *
 * This writes the signature and signer data to the FDT
 *
 * @blob: pointer to the FDT header
 * @noffset: hash node offset
 * @value: signature value to be set
 * @value_len: signature value length
 * @comment: Text comment to write (NULL for none)
 *
 * returns
 *     0, on success
 *     -FDT_ERR_..., on failure
 */
static int fdt_write_sig(void *blob, uint8_t *value, int value_len,
			 const char *sig_name, const char *comment,
			 const char *cmdname)
{
	uint32_t strdata[2];
	int string_size;
	int sigs_node, noffset;
	int ret;

	/*
	 * Get the current string size, before we update the FIT and add
	 * more
	 */
	string_size = fdt_size_dt_strings(blob);

	sigs_node = fdt_subnode_offset(blob, 0, FIT_SIG_NODENAME);
	if (sigs_node == -FDT_ERR_NOTFOUND)
		sigs_node = fdt_add_subnode(blob, 0, FIT_SIG_NODENAME);
	if (sigs_node < 0)
		return sigs_node;

	/* Create a node for this signature */
	noffset = fdt_subnode_offset(blob, sigs_node, sig_name);
	if (noffset == -FDT_ERR_NOTFOUND)
		noffset = fdt_add_subnode(blob, sigs_node, sig_name);
	if (noffset < 0)
		return noffset;

	ret = fdt_setprop(blob, noffset, FIT_VALUE_PROP, value, value_len);
	if (!ret) {
		ret = fdt_setprop_string(blob, noffset, "signer-name",
					 "fdt_sign");
	}
	if (!ret) {
		ret = fdt_setprop_string(blob, noffset, "signer-version",
					 PLAIN_VERSION);
	}
	if (comment && !ret)
		ret = fdt_setprop_string(blob, noffset, "comment", comment);
	if (!ret) {
		time_t timestamp = imagetool_get_source_date(cmdname,
							     time(NULL));
		uint32_t t = cpu_to_uimage(timestamp);

		ret = fdt_setprop(blob, noffset, FIT_TIMESTAMP_PROP, &t,
			sizeof(uint32_t));
	}

	/* This is a legacy offset, it is unused, and must remain 0. */
	strdata[0] = cpu_to_fdt32(string_size);
	if (!ret) {
		ret = fdt_setprop(blob, noffset, "hashed-strings",
				  strdata, sizeof(strdata));
	}

	return ret;
}

static int fdt_process_sig(const char *keydir, const char *keyfile,
			   void *keydest, void *blob, const char *keyname,
			   const char *comment, int require_keys,
			   const char *engine_id, const char *cmdname)
{
	struct image_sign_info info;
	struct image_region *region;
	int region_count;
	uint8_t *value;
	uint value_len;
	int ret;

	ret = fdt_get_regions(blob, &region, &region_count);
	if (ret)
		return ret;
	ret = fdt_setup_sig(&info, keydir, keyfile, keyname, blob,
			    "sha256,rsa2048", NULL, require_keys ? "fdt" : NULL,
			    engine_id);
	if (ret)
		return ret;

	ret = info.crypto->sign(&info, region, region_count, &value,
				&value_len);
	free(region);
	if (ret) {
		fprintf(stderr, "Failed to sign FDT\n");

		/* We allow keys to be missing */
		if (ret == -ENOENT)
			return 0;
		return -1;
	}

	ret = fdt_write_sig(blob, value, value_len, keyname, comment, cmdname);
	if (ret) {
		if (ret == -FDT_ERR_NOSPACE)
			return -ENOSPC;
		printf("Can't write signature: %s\n", fdt_strerror(ret));
		return -1;
	}
	free(value);

	/* Write the public key into the supplied FDT file */
	if (keydest) {
		ret = info.crypto->add_verify_data(&info, keydest);
		if (ret) {
			printf("Failed to add verification data\n");
		}
		return ret;
	}

	return 0;
}

int fdt_add_verif_data(const char *keydir, const char *keyfile, void *keydest,
		       void *blob, const char *keyname, const char *comment,
		       bool require_keys, const char *engine_id,
		       const char *cmdname)
{
	char def_name[80];
	int ret;

	/*
	 * If we don't have a signature name, try to make one from the keyfile.
	 * '/path/to/dir/name.key' becomes 'name'
	 */
	if (!keyname) {
		const char *p = strrchr(keyfile, '/');
		char *q;

		if (p)
			p++;
		else
			p = keyfile;
		strncpy(def_name, p, sizeof(def_name));
		def_name[sizeof(def_name) - 1] = '\0';
		q = strstr(def_name, ".key");
		if (q && q[strlen(q)] == '\0')
			*q = '\0';
		keyname = def_name;
	}
	ret = fdt_process_sig(keydir, keyfile, keydest, blob, keyname, comment,
			      require_keys, engine_id, cmdname);
	if (ret)
		return ret;

	return 0;
}
