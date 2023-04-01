// SPDX-License-Identifier: GPL-2.0
/*
 * Modified from coreboot bochs.c
 */

#define LOG_DEBUG
#define LOG_CATEGORY	UCLASS_VIDEO

#include <common.h>
#include <dm.h>
#include <log.h>
#include <pci.h>
#include <video.h>
#include <asm/io.h>
#include <asm/mtrr.h>
#include <linux/sizes.h>
#include "bochs.h"

#define VGA_INDEX			0x3c0

#define IOPORT_INDEX          0x01ce
#define IOPORT_DATA           0x01cf

enum {
	INDEX_ID,
	INDEX_XRES,
	INDEX_YRES,
	INDEX_BPP,
	INDEX_ENABLE,
	INDEX_BANK,
	INDEX_VIRT_WIDTH,
	INDEX_VIRT_HEIGHT,
	INDEX_X_OFFSET,
	INDEX_Y_OFFSET,
	INDEX_VIDEO_MEMORY_64K
};

#define ID0		0xb0c0

#define ENABLED		BIT(0)
#define LFB_ENABLED	BIT(6)
#define NOCLEARMEM	BIT(7)

#define MMIO_BASE	0x500

static int xsize = CONFIG_VIDEO_BOCHS_SIZE_X;
static int ysize = CONFIG_VIDEO_BOCHS_SIZE_Y;

static void bochs_write(void *mmio, int index, int val)
{
	writew(val, mmio + MMIO_BASE + index * 2);
}

static int bochs_read(void *mmio, int index)
{
	return readw(mmio + MMIO_BASE + index * 2);
}

static void bochs_vga_write(int index, uint8_t val)
{
	outb(val, VGA_INDEX);
}

static int bochs_init_linear_fb(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	ulong fb;
	void *mmio;
	int id, mem;

	log_debug("here\n");
	log_debug("probing %s at PCI %x\n", dev->name, dm_pci_get_bdf(dev));
	fb = dm_pci_read_bar32(dev, 0);
	if (!fb)
		return log_msg_ret("fb", -EIO);

	/* MMIO bar supported since qemu 3.0+ */
	mmio = dm_pci_map_bar(dev, PCI_BASE_ADDRESS_2, 0, 0, PCI_REGION_TYPE,
			      PCI_REGION_MEM);

	if (!mmio)
		return log_msg_ret("map", -EIO);

	log_debug("QEMU VGA: bochs @ %p: %d MiB FB at %lx\n", mmio, mem / SZ_1M,
		  fb);

	/* bochs dispi detection */
	id = bochs_read(mmio, INDEX_ID);
	if ((id & 0xfff0) != ID0) {
		log_debug("ID mismatch\n");
		return -EPROTONOSUPPORT;
	}
	mem = bochs_read(mmio, INDEX_VIDEO_MEMORY_64K) * SZ_64K;

	uc_priv->xsize = xsize;
	uc_priv->ysize = ysize;
	uc_priv->bpix = VIDEO_BPP32;

	/* setup video mode */
	bochs_write(mmio, INDEX_ENABLE,  0);
	bochs_write(mmio, INDEX_BANK,  0);
	bochs_write(mmio, INDEX_BPP, VNBITS(uc_priv->bpix));
	bochs_write(mmio, INDEX_XRES, xsize);
	bochs_write(mmio, INDEX_YRES, ysize);
	bochs_write(mmio, INDEX_VIRT_WIDTH, xsize);
	bochs_write(mmio, INDEX_VIRT_HEIGHT, ysize);
	bochs_write(mmio, INDEX_X_OFFSET, 0);
	bochs_write(mmio, INDEX_Y_OFFSET, 0);
	bochs_write(mmio, INDEX_ENABLE, ENABLED | LFB_ENABLED);

	bochs_vga_write(0, 0x20);	/* disable blanking */

	plat->base = fb;

	return 0;
}

static int bochs_video_probe(struct udevice *dev)
{
	int ret;

	ret = bochs_init_linear_fb(dev);
	if (ret)
		return log_ret(ret);

	return 0;
}

static int bochs_video_bind(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);

	/* Set the maximum supported resolution */
	uc_plat->size = 2560 * 1600 * 4;
	log_debug("%s: Frame buffer size %x\n", __func__, uc_plat->size);

	return 0;
}

static const struct udevice_id bochs_video_ids[] = {
	{ .compatible = "bochs-fb" },
	{ }
};

U_BOOT_DRIVER(bochs_video) = {
	.name	= "bochs_video",
	.id	= UCLASS_VIDEO,
	.of_match = bochs_video_ids,
	.bind	= bochs_video_bind,
	.probe	= bochs_video_probe,
};

static struct pci_device_id bochs_video_supported[] = {
	{ PCI_DEVICE(0x1234, 0x1111) },
	{ },
};

U_BOOT_PCI_DEVICE(bochs_video, bochs_video_supported);
