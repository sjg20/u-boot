// SPDX-License-Identifier: GPL-2.0+
/*
 * Implements boot devices, typically MMC/NVMe, used to hold the kernel
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY	LOGC_VBOOT

#include <common.h>
#include <bootstage.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <usb.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>
#include <dm/device-internal.h>

/* Maximum number of devices we can support */
enum {
	MAX_DISK_INFO	= 10,
};

/**
 * Add a device to info array if it matches the supplied disk_flags
 *
 * @name: Peripheral name
 * @dev: Device to check
 * @req_flags: Requested flags which must be present for each device
 * @info: Output array of matching devices
 * @return 0 if added, -ENOENT if not
 */
static int add_matching_device(struct udevice *dev, u32 req_flags,
			       VbDiskInfo *info)
{
	struct blk_desc *bdev = dev_get_uclass_plat(dev);
	u32 flags;

	/* Ignore zero-length devices */
	if (!bdev->lba) {
		log_debug("Ignoring %s: zero-length\n", dev->name);
		return -ENOENT;
	}

	/*
	 * Only add this storage device if the properties of req_flags is a
	 * subset of the properties of flags.
	 */
	flags = bdev->removable ? VB_DISK_FLAG_REMOVABLE : VB_DISK_FLAG_FIXED;
	if ((flags & req_flags) != req_flags) {
		log_debug("Ignoring %s: flags=%x, req_flags=%x\n", dev->name,
			  flags, req_flags);
		return -ENOENT;
	}

	info->handle = (VbExDiskHandle_t)dev;
	info->bytes_per_lba = bdev->blksz;
	info->lba_count = bdev->lba;
	info->flags = flags | VB_DISK_FLAG_EXTERNAL_GPT;
	info->name = dev->name;

	return 0;
}

/**
 * boot_device_usb_start() - Start up USB and (re)scan the bus
 *
 * This sets vboot->usb_is_enumerated to true if the enumeration succeeds
 *
 * @vboot: vboot info
 * @return 0 (always)
 */
static int boot_device_usb_start(struct vboot_info *vboot)
{
	bool enumerate = true;

	/*
	 * if the USB devices have already been enumerated, redo it
	 * only if something has been plugged on unplugged.
	 */
	if (vboot->usb_is_enumerated)
		enumerate = usb_detect_change();

	if (enumerate) {
		/*
		 * We should stop all USB devices first. Otherwise we can't
		 * detect any new devices.
		 */
		usb_stop();

		if (usb_init() >= 0)
			vboot->usb_is_enumerated = true;
	}

	return 0;
}

vb2_error_t VbExDiskGetInfo(VbDiskInfo **infos_ptr, u32 *count_ptr,
			  u32 disk_flags)
{
	struct vboot_info *vboot = vboot_get();
	VbDiskInfo *infos;
	u32 max_count;	/* maximum number of devices to scan for */
	u32 count = 0;	/* number of matching devices found */
	struct udevice *dev;
	struct uclass *uc;

	/* We return as many disk infos as possible */
	max_count = MAX_DISK_INFO;

	infos = calloc(max_count, sizeof(VbDiskInfo));

	bootstage_start(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_INFO,
			"boot_device_info");

	/* If we are looking for removable disks, scan USB */
	if (IS_ENABLED(CONFIG_USB) && (disk_flags & VB_DISK_FLAG_REMOVABLE))
		boot_device_usb_start(vboot);

	/* Scan through all the interfaces looking for devices */
	uclass_id_foreach_dev(UCLASS_BLK, dev, uc) {
		int ret;

		ret = device_probe(dev);
		if (ret)
			continue;
		/* Now record the devices that have the required flags */
		if (!add_matching_device(dev, disk_flags, infos + count))
			count++;
		if (count == max_count)
			log_warning("Reached maximum device count\n");
	}

	if (count) {
		*infos_ptr = infos;
		*count_ptr = count;
	} else {
		*infos_ptr = NULL;
		*count_ptr = 0;
		free(infos);
	}
	bootstage_accum(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_INFO);
	log_debug("Found %u disks\n", count);

	/* The operation itself succeeds, despite scan failure all about */
	return VB2_SUCCESS;
}

vb2_error_t VbExDiskFreeInfo(VbDiskInfo *infos, VbExDiskHandle_t preserve_handle)
{
	/* We do nothing for preserve_handle as we keep all the devices on */
	free(infos);

	return VB2_SUCCESS;
}

vb2_error_t VbExDiskRead(VbExDiskHandle_t handle, u64 lba_start, u64 lba_count,
		       void *buffer)
{
	struct udevice *dev = (struct udevice *)handle;
	struct blk_desc *bdev = dev_get_uclass_plat(dev);
	u64 blks_read;

	log_debug("lba_start=%x, lba_count=%x, buffer=%p\n", (uint)lba_start,
		  (uint)lba_count, buffer);

	if (lba_start >= bdev->lba || lba_start + lba_count > bdev->lba)
		return VB2_ERROR_UNKNOWN;

	/* Keep track of the total time spent reading */
	bootstage_start(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_READ,
			"boot_device_read");
	blks_read = blk_dread(bdev, lba_start, lba_count, buffer);
	bootstage_accum(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_READ);
	if (blks_read != lba_count)
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

vb2_error_t VbExDiskWrite(VbExDiskHandle_t handle, u64 lba_start,
			u64 lba_count, const void *buffer)
{
	struct udevice *dev = (struct udevice *)handle;
	struct blk_desc *bdev = dev_get_uclass_plat(dev);

	if (lba_start >= bdev->lba || lba_start + lba_count > bdev->lba)
		return VB2_ERROR_UNKNOWN;

	if (blk_dwrite(bdev, lba_start, lba_count, buffer) != lba_count)
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

/*
 * Simple implementation of new streaming APIs.  This turns them into calls to
 * the sector-based disk read/write functions above.  This will be replaced
 * imminently with fancier streaming.  In the meantime, this will allow the
 * vboot_reference change which uses the streaming APIs to commit.
 */

/* The stub implementation assumes 512-byte disk sectors */
#define LBA_BYTES 512

/**
 * struct disk_stream - struct for simulating a stream for sector-based disks
 *
 * @handle: Disk handle, as passed to VbExDiskRead()
 * @sector: Next sector to read
 * @sectors_left: Number of sectors left in partition
 */
struct disk_stream {

	VbExDiskHandle_t handle;
	u64 sector;
	u64 sectors_left;
};

vb2_error_t VbExStreamOpen(VbExDiskHandle_t handle, u64 lba_start,
			 u64 lba_count, VbExStream_t *stream)
{
	struct disk_stream *s;

	*stream = NULL;
	if (!handle)
		return VB2_ERROR_UNKNOWN;

	s = malloc(sizeof(*s));
	if (!s)
		return VB2_ERROR_UNKNOWN;
	s->handle = handle;
	s->sector = lba_start;
	s->sectors_left = lba_count;

	*stream = (void *)s;

	return VB2_SUCCESS;
}

vb2_error_t VbExStreamRead(VbExStream_t stream, u32 bytes, void *buffer)
{
	struct disk_stream *s = (struct disk_stream *)stream;
	u64 sectors;
	vb2_error_t rv;

	if (!s)
		return VB2_ERROR_UNKNOWN;

	/* For now, require reads to be a multiple of the LBA size */
	if (bytes % LBA_BYTES)
		return VB2_ERROR_UNKNOWN;

	/* Fail on overflow */
	sectors = bytes / LBA_BYTES;
	if (sectors > s->sectors_left)
		return VB2_ERROR_UNKNOWN;

	rv = VbExDiskRead(s->handle, s->sector, sectors, buffer);
	if (rv != VB2_SUCCESS)
		return rv;

	s->sector += sectors;
	s->sectors_left -= sectors;

	return VB2_SUCCESS;
}

void VbExStreamClose(VbExStream_t stream)
{
	struct disk_stream *s = (struct disk_stream *)stream;

	free(s);
}
