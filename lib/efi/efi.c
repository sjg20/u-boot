// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 *
 * EFI information obtained here:
 * http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES
 *
 * Common EFI functions
 */

#include <common.h>
#include <blk.h>
#include <debug_uart.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <linux/err.h>
#include <linux/types.h>
#include <efi.h>
#include <efi_api.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <dm/root.h>

/*
 * Global declaration of gd.
 *
 * As we write to it before relocation we have to make sure it is not put into
 * a .bss section which may overlap a .rela section. Initialization forces it
 * into a .data section which cannot overlap any .rela section.
 */
struct global_data *global_data_ptr = (struct global_data *)~0;

/*
 * Unfortunately we cannot access any code outside what is built especially
 * for the stub. lib/string.c is already being built for the U-Boot payload
 * so it uses the wrong compiler flags. Add our own memset() here.
 */
static void efi_memset(void *ptr, int ch, int size)
{
	char *dest = ptr;

	while (size-- > 0)
		*dest++ = ch;
}

/*
 * Since the EFI stub cannot access most of the U-Boot code, add our own
 * simple console output functions here. The EFI app will not use these since
 * it can use the normal console.
 */
void efi_putc(struct efi_priv *priv, const char ch)
{
	struct efi_simple_text_output_protocol *con = priv->sys_table->con_out;
	uint16_t ucode[2];

	ucode[0] = ch;
	ucode[1] = '\0';
	con->output_string(con, ucode);
}

void efi_puts(struct efi_priv *priv, const char *str)
{
	while (*str)
		efi_putc(priv, *str++);
}

int efi_init(struct efi_priv *priv, const char *banner, efi_handle_t image,
	     struct efi_system_table *sys_table)
{
	efi_guid_t loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	struct efi_boot_services *boot = sys_table->boottime;
	struct efi_loaded_image *loaded_image;
	int ret;

	efi_memset(priv, '\0', sizeof(*priv));
	priv->sys_table = sys_table;
	priv->boot = sys_table->boottime;
	priv->parent_image = image;
	priv->run = sys_table->runtime;

	efi_puts(priv, "U-Boot EFI ");
	efi_puts(priv, banner);
	efi_putc(priv, ' ');

	ret = boot->open_protocol(priv->parent_image, &loaded_image_guid,
				  (void **)&loaded_image, priv->parent_image,
				  NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (ret) {
		efi_puts(priv, "Failed to get loaded image protocol\n");
		return ret;
	}
	priv->image_data_type = loaded_image->image_data_type;

	return 0;
}

void *efi_malloc(struct efi_priv *priv, int size, efi_status_t *retp)
{
	struct efi_boot_services *boot = priv->boot;
	void *buf = NULL;

	*retp = boot->allocate_pool(priv->image_data_type, size, &buf);

	return buf;
}

void efi_free(struct efi_priv *priv, void *ptr)
{
	struct efi_boot_services *boot = priv->boot;

	boot->free_pool(ptr);
}

/**
 * Create a block device so U-Boot can access an EFI device
 *
 * @handle:	EFI handle to bind
 * @blkio:	block io protocol
 * Return:	0 = success
 */
int efi_bind_block(efi_handle_t handle, struct efi_block_io *blkio)
{
	struct efi_media_plat plat;
	struct udevice *dev;
	char name[18];
	int ret;

	plat.handle = handle;
	plat.blkio = blkio;
	ret = device_bind(dm_root(), DM_DRIVER_GET(efi_media), "efi_media",
			  &plat, ofnode_null(), &dev);
	if (ret)
		return log_msg_ret("bind", ret);

	snprintf(name, sizeof(name), "efi_media_%x", dev_seq(dev));
	device_set_name(dev, name);

	return 0;
}
