// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_VBOOT

#include <common.h>
#include <blk.h>
#include <bloblist.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <log.h>
#include <mapmem.h>
#include <uuid.h>
#ifdef CONFIG_X86
#include <asm/bootm.h>
#include <asm/zimage.h>
#endif
#include <cros/cros_common.h>
#include <cros/vboot.h>
#include <dm/device-internal.h>

/*
 * The Chrome OS kernel file has the following format:
 *
 *  0		Vboot header (used by the vboot library). At offset 4f0 is the
 *		bootloader address, assuming that the kernel (at 8000) is loaded
 *		at CROS_32BIT_ENTRY_ADDR
 *		This header is easy to recognise as the first bytes are
 *		"CHROMEOS"
 *  8000	Kernel start
 *  BLO - 1000	Setup block (x86)
 *  BLO - 2000	Command line
 */

enum {
	CROS_32BIT_ENTRY_ADDR = 0x100000
};

/* Maximum kernel command-line size */
#define CMDLINE_SIZE	4096

/* Size of the x86 zeropage table */
#define CROS_PARAMS_SIZE	4096

/* Extra buffer to string replacement */
#define EXTRA_BUFFER		4096

/* Pointer to the vboot information so we can handle ft_board_setup() */
struct vboot_info *boot_kernel_vboot_ptr;

/**
 * This loads kernel command line from the buffer that holds the loaded kernel
 * image. This function calculates the address of the command line from the
 * bootloader address.
 *
 * @kernel_buffer: Address of kernel buffer in memory
 * @bootloader_offset: Offset of bootloader in kernel_buffer
 * @return kernel config address
 */
static char *get_kernel_config(void *kernel_buffer, size_t bootloader_offset)
{
	/* Use the bootloader address to find the kernel config location */
	return kernel_buffer + bootloader_offset -
		(CROS_PARAMS_SIZE + CMDLINE_SIZE);
}

static u32 get_dev_num(const struct udevice *dev)
{
	const struct blk_desc *desc = dev_get_uclass_platdata(dev);

	return desc->devnum;
}

/**
 * This replaces:
 *   %D -> device number
 *   %P -> partition number
 *   %U -> GUID
 * in kernel command line.
 *
 * For example:
 *   ("root=/dev/sd%D%P", 2, 3)      -> "root=/dev/sdc3"
 *   ("root=/dev/mmcblk%Dp%P", 0, 5) -> "root=/dev/mmcblk0p5".
 *
 * @src: Input string
 * @devnum: Device number of the storage device we will mount
 * @partnum: Partition number of the root file system we will mount
 * @guid: GUID of the kernel partition as a string
 * @dst: Output string
 * @dst_size: Size of output string
 * @return zero if it succeeds, non-zero if it fails
 */
static int update_cmdline(char *src, int devnum, int partnum, char *guid,
			  char *dst, int dst_size)
{
	char *dst_end = dst + dst_size;
	int c;

	/* sanity check on inputs */
	if (devnum < 0 || devnum > 25 || partnum < 1 || partnum > 99 ||
	    dst_size < 0 || dst_size > 10000) {
		log_err("insane input: %d, %d, %d\n", devnum, partnum,
			dst_size);
		return 1;
	}

	/*
	 * Condition "dst + X <= dst_end" checks if there is at least X bytes
	 * left in dst. We use X > 1 so that there is always 1 byte for '\0'
	 * after the loop.
	 *
	 * We constantly estimate how many bytes we are going to write to dst
	 * for copying characters from src or for the string replacements, and
	 * check if there is sufficient space.
	 */

#define CHECK_SPACE(bytes) \
	if (!(dst + (bytes) <= dst_end)) { \
		log_debug("fail: need at least %d bytes\n", (bytes)); \
		return 1; \
	}

	while ((c = *src++)) {
		if (c != '%') {
			CHECK_SPACE(2);
			*dst++ = c;
			continue;
		}

		switch ((c = *src++)) {
		case '\0':
			log_debug("mal-formed input: end in '%%'\n");
			return 1;
		case 'D':
			/*
			 * TODO: Do we have any better way to know whether %D
			 * is replaced by a letter or digits? So far, this is
			 * done by a rule of thumb that if %D is followed by a
			 * 'p' character, then it is replaced by digits.
			 */
			if (*src == 'p') {
				CHECK_SPACE(3);
				strcpy(dst, simple_itoa(devnum));
			} else {
				CHECK_SPACE(2);
				*dst++ = 'a' + devnum;
			}
			break;
		case 'P':
			CHECK_SPACE(3);
			strcpy(dst, simple_itoa(partnum));
			break;
		case 'U':
			/* GUID replacement needs 36 bytes */
			CHECK_SPACE(UUID_STR_LEN + 1);
			strncpy(dst, guid, UUID_STR_LEN);
			dst += UUID_STR_LEN;
			break;
		default:
			CHECK_SPACE(3);
			*dst++ = '%';
			*dst++ = c;
			break;
		}
	}

#undef CHECK_SPACE

	*dst = '\0';
	return 0;
}

static int boot_kernel(struct vboot_info *vboot,
		       VbSelectAndLoadKernelParams *kparams)
{
	/* sizeof(CHROMEOS_BOOTARGS) reserves extra 1 byte */
	char cmdline_buf[sizeof(CHROMEOS_BOOTARGS) + CMDLINE_SIZE];
	/* Reserve EXTRA_BUFFER bytes for update_cmdline's string replacement */
// 	char cmdline_out[sizeof(CHROMEOS_BOOTARGS) + CMDLINE_SIZE +
// 		EXTRA_BUFFER];
	char *cmdline;
	struct udevice *dev;
	char guid[UUID_STR_LEN + 1];
#ifdef CONFIG_X86
	struct boot_params *params;
#endif

#ifndef CONFIG_X86
	/* Chromium OS kernel has to be loaded at fixed location */
	struct cmd_tbl cmdtp;
	ulong addr = map_to_sysmem(kparams->kernel_buffer);
	char address[20];
	char *argv[] = { "bootm", address };

	sprintf(address, "%08lx", addr);
#endif
	strcpy(cmdline_buf, CHROMEOS_BOOTARGS);

	/*
	 * bootloader_address is the offset in kernel image plus kernel body
	 * load address; so subtract this address from bootloader_address and
	 * you have the offset.
	 *
	 * Note that kernel body load address is kept in kernel preamble but
	 * actually serves no real purpose; for one, the kernel buffer is not
	 * always allocated at that address (nor even recommended to be).
	 *
	 * Because this address does not affect kernel-buffer location (or in
	 * fact anything else), the current consensus is not to adjust this
	 * address on a per-board basis.
	 *
	 * If for any unforeseeable reason this address is going to be not
	 * CROS_32BIT_ENTRY_ADDR=0x100000, please also update the code here.
	 */
	cmdline = get_kernel_config(kparams->kernel_buffer,
				    kparams->bootloader_address -
				    CROS_32BIT_ENTRY_ADDR);
	/*
	 * strncat could write CMDLINE_SIZE + 1 bytes to cmdline_buf. This
	 * is okay because the extra 1 byte has been reserved in sizeof().
	 */
	strncat(cmdline_buf, cmdline, CMDLINE_SIZE);

#ifdef LOG_DEBUG
	printf("cmdline before update: ptr=%p, len %dn", cmdline_buf,
	       strlen(cmdline_buf));
	puts(cmdline_buf);
	printf("\n");
#endif

	uuid_bin_to_str(kparams->partition_guid, guid, UUID_STR_FORMAT_GUID);
	log_info("partition_number=%d, guid=%s\n", kparams->partition_number,
		 guid);

	if (update_cmdline(cmdline_buf, get_dev_num(kparams->disk_handle),
			   kparams->partition_number + 1, guid, cmdline,
			   CMDLINE_SIZE)) {
		log_err("failed replace %%[DUP] in command line\n");
		return 1;
	}

#ifdef LOG_DEBUG
	printf("cmdline after update: ptr=%p, len %d\n", cmdline,
	       strlen(cmdline));
	puts(cmdline);
	printf("\n");
#endif

	env_set("bootargs", cmdline);

	boot_kernel_vboot_ptr = vboot;

	/*
	 * Disable keyboard and flush buffer so that further keystrokes
	 * won't interfere kernel driver init
	 */
	uclass_first_device(UCLASS_KEYBOARD, &dev);
	if (dev)
		device_remove(dev, DM_REMOVE_NORMAL);

	log_info("Bloblist:\n");
	bloblist_show_list();
#ifdef CONFIG_X86
	vboot_update_acpi(vboot);

	params = (struct boot_params *)(cmdline + CMDLINE_SIZE);
	log_debug("kernel_buffer=%p, size=%x, bootloader_address=%llx, size=%x, cmdline=%p, params=%p\n",
		  kparams->kernel_buffer, kparams->kernel_buffer_size,
		  kparams->bootloader_address, kparams->bootloader_size,
		  cmdline, params);
#ifdef LOG_DEBUG
	print_buffer((ulong)params + 0x1f1, (void *)params + 0x1f1, 1, 0xf, 0);
#endif
	if (!setup_zimage(params, cmdline, 0, 0, 0, 0)) {
#ifdef LOG_DEBUG
		zimage_dump(params);
		print_buffer((ulong)kparams->kernel_buffer,
			     kparams->kernel_buffer, 1, 0x100, 0);
		log_debug("go %p, %p\n", params, kparams->kernel_buffer);
#endif
		boot_linux_kernel((ulong)params, (ulong)kparams->kernel_buffer,
				  false);
	}
#else
	cmdtp.name = "bootm";
	do_bootm(&cmdtp, 0, ARRAY_SIZE(argv), argv);
#endif
	boot_kernel_vboot_ptr = NULL;

	log_debug("failed to boot; is kernel broken?\n");
	return 1;
}

int vboot_rw_boot_kernel(struct vboot_info *vboot)
{
	int ret;

	bootstage_mark(BOOTSTAGE_VBOOT_DONE);

	ret = boot_kernel(vboot, &vboot->kparams);
	if (ret)
		return log_msg_ret("Kernel boot failed", ret);

	return 0;
}
