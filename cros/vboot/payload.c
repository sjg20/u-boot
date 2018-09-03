// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google Inc.
 */

#define LOG_CATEGORY	LOGC_VBOOT

#include <common.h>
#include <abuf.h>
#include <cbfs.h>
#include <dm.h>
#include <log.h>
#include <cros/crossystem.h>
#include <cros/payload.h>
#include <cros/vbfile.h>
#include <cros/vboot.h>
#include <dm/root.h>
#include <linux/list.h>
#include <lzma/LzmaTypes.h>
#include <lzma/LzmaTools.h>

/*
#include <stdbool.h>
#include <libpayload.h>
#include <lzma.h>
#include <vb2_sha.h>
#include <cbfs.h>

#include "arch/cache.h"
#include "base/cleanup_funcs.h"
#include "drivers/flash/cbfs.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "boot/payload.h"
#include "vboot/crossystem/crossystem.h"
*/

/* List of alternate bootloaders */
static struct list_head *altfw_head;

/* Media to use for reading payloads */
// static struct cbfs_media cbfs_media;
// static bool cbfs_media_valid;

#define PAYLOAD_HASH_SUFFIX	".sha256"
#define PAYLOAD_SECTION		"RW_LEGACY"

/*
 * get_payload_hash() - Obtain the hash for a given payload.
 *    Given the name of a payload (e.g., "altfw/XXX") appends
 *    ".sha256" to the name (e.g., "altfw/XXX.sha256") and
 *    provides a buffer of length VB2_SHA256_DIGEST_SIZE that
 *    contains the file's contents.
 * @payload_name: Name of payload
 * @return pointer to buffer on success, otherwise NULL.
 *     Caller is responsible for freeing the buffer.
 */

static uint8_t *get_payload_hash(const char *payload_name)
{
	size_t full_name_len = strlen(payload_name) +
			       sizeof(PAYLOAD_HASH_SUFFIX);
	struct vboot_info *vboot = vboot_get();
	void *data;
	size_t data_size = 0;
	char *full_name;
	struct abuf buf;
	int ret;

	full_name = xzalloc(full_name_len);
	snprintf(full_name, full_name_len, "%s%s", payload_name,
		 PAYLOAD_HASH_SUFFIX);

	abuf_init(&buf);
	ret = vbfile_load(vboot, full_name, &buf);
	free(full_name);
	if (ret) {
		log_err("Could not find hash for %s in default media cbfs.\n",
		       payload_name);
		return NULL;
	}
	if (abuf_size(&buf) != VB2_SHA256_DIGEST_SIZE) {
		printf("Size of hash for %s is not %u: %u\n",
		       payload_name, VB2_SHA256_DIGEST_SIZE,
		       (uint32_t)data_size);
		abuf_uninit(&buf);
		return NULL;
	}
	data = abuf_uninit_move(&buf, &data_size);
	if (!data) {
		log_err("Out of memory\n");
		return NULL;
	}

	return (uint8_t *)data;
}

/*
 * payload_load() - Load an image from the given payload
 *
 * @payload: Pointer to payload containing the image to load
 * @entryp: Returns pointer to the entry point
 * @return 0 i OK, -1 on error
 */
static int payload_load(struct cbfs_payload *payload, void **entryp)
{
	struct cbfs_payload_segment *seg = &payload->segments;
	char *base = (void *)seg;

	/* Loop until we find an entry point, then return it */
	while (1) {
		void *src = base + be32_to_cpu(seg->offset);
		void *dst = (void *)(unsigned long)be64_to_cpu(seg->load_addr);
		u32 src_len = be32_to_cpu(seg->len);
		u32 dst_len = be32_to_cpu(seg->mem_len);
		int comp = be32_to_cpu(seg->compression);

		switch (seg->type) {
		case PAYLOAD_SEGMENT_CODE:
		case PAYLOAD_SEGMENT_DATA:
			printf("CODE/DATA: dst=%p dst_len=%d src=%p src_len=%d compression=%d\n",
			       dst, dst_len, src, src_len, comp);
			switch (comp) {
			case CBFS_COMPRESS_NONE:
				if (dst_len < src_len) {
					printf("Output buffer too small.\n");
					return -1;
				}
				memcpy(dst, src, src_len);
				break;
			case CBFS_COMPRESS_LZMA: {
				SizeT inout_size = dst_len;
				int ret;

				ret = lzmaBuffToBuffDecompress(dst, &inout_size,
							       src, src_len);
				if (ret) {
					log_err("LZMA: Decompression failed (err-%d)\n",
						ret);
					return -1;
				}
				break;
			}
			default:
				printf("Compression type %x not supported\n",
				       comp);
				return -1;
			}
			break;
		case PAYLOAD_SEGMENT_BSS:
			printf("BSS: dst=%p len=%d\n", dst, dst_len);
			memset(dst, 0, dst_len);
			break;
		case PAYLOAD_SEGMENT_PARAMS:
			printf("PARAMS: skipped\n");
			break;
		case PAYLOAD_SEGMENT_ENTRY:
			*entryp = dst;
			return 0;
		default:
			printf("segment type %x not implemented. Exiting\n",
			       seg->type);
			return -1;
		}
		seg++;
	}
}

int payload_run(const char *payload_name, int verify)
{
	struct vboot_info *vboot = vboot_get();
	struct cbfs_payload *payload;
	size_t payload_size = 0;
	void (*entry_func)(void);
	struct abuf buf;
	void *entry;
	int ret;

	abuf_init(&buf);
	ret = vbfile_section_load(vboot, PAYLOAD_SECTION, payload_name, &buf);
	if (ret) {
		log_err("Could not find '%s'\n", payload_name);
		return 1;
	}

	if (verify) {
		uint8_t real_hash[VB2_SHA256_DIGEST_SIZE];
		uint8_t *expected_hash;
		vb2_error_t rv;

		/* Calculate hash of payload. */
		rv = vb2_digest_buffer((const uint8_t *)payload,
				       payload_size, VB2_HASH_SHA256,
				       real_hash, sizeof(real_hash));
		if (rv) {
			printf("SHA-256 calculation failed for "
			       "%s payload.\n", payload_name);
			free(payload);
			return 1;
		}

		/* Retrieve the expected hash of payload stored in AP-RW. */
		expected_hash = get_payload_hash(payload_name);
		if (expected_hash == NULL) {
			printf("Could not retrieve expected hash of "
			       "%s payload.\n", payload_name);
			free(payload);
			return 1;
		}

		ret = memcmp(real_hash, expected_hash, sizeof(real_hash));
		free(expected_hash);
		if (ret != 0) {
			printf("%s payload hash check failed!\n", payload_name);
			free(payload);
			return 1;
		}
		printf("%s payload hash check succeeded.\n", payload_name);
	}

	printf("Loading %s into RAM\n", payload_name);
	ret = payload_load(payload, &entry);
	free(payload);
	if (ret) {
		printf("Failed: error %d\n", ret);
		return 1;
	}
	ret = crossystem_setup(vboot, FIRMWARE_TYPE_LEGACY);
	if (ret)
		log_warning("Failed to set up crossystem data\n");

	/* TODO(sjg@chromium.org): Use bootm stuff for this */
	/*
	 * Call remove function of all devices with a removal flag set.
	 * This may be useful for last-stage operations, like cancelling
	 * of DMA operation or releasing device internal buffers.
	 */
	dm_remove_devices_flags(DM_REMOVE_ACTIVE_ALL | DM_REMOVE_NON_VITAL);

	/* Remove all active vital devices next */
	dm_remove_devices_flags(DM_REMOVE_ACTIVE_ALL);

	printf("Starting %s at %p...", payload_name, entry);
	cleanup_before_linux();
	entry_func = entry;
	entry_func();

	printf("%s returned, unfortunately", payload_name);

	return 1;
}

static struct list_head *get_altfw_list(void)
{
	struct vboot_info *vboot = vboot_get();
	char *loaders, *ptr;
	struct list_head *head, *tail;
	struct abuf buf;
	int ret;

	/* Load alternate bootloader list from cbfs */
	abuf_init(&buf);
	ret = vbfile_section_load(vboot, PAYLOAD_SECTION, "altfw/list", &buf);
	if (ret) {
		log_info("altfw list not found (err=%d)\n", ret);
		return NULL;
	}

	printf("%s: Supported alternate bootloaders:\n", __func__);
	ptr = loaders;
	head = xzalloc(sizeof(*head));
	tail = head;
	do {
		struct altfw_info *node;
		const char *seqnum;
		char *line;

		line = strsep(&ptr, "\n");
		if (!line)
			break;
		node = xzalloc(sizeof (*node));
		seqnum = strsep(&line, ";");
		if (!seqnum)
			break;
		node->seqnum = simple_strtol(seqnum, NULL, 10);

		node->filename = strsep(&line, ";");
		node->name = strsep(&line, ";");
		node->desc = strsep(&line, ";");
		if (!node->desc)
			break;
		printf("   %d %-15s %-15s %s\n", node->seqnum, node->name,
		       node->filename, node->desc);
		list_add_tail(&node->list_node, tail);
		tail = &node->list_node;
	} while (1);
	abuf_uninit(&buf);

	return head;
}

struct list_head *payload_get_altfw_list(void)
{
	if (!altfw_head)
		altfw_head = get_altfw_list();

	return altfw_head;
}
