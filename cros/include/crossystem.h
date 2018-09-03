/*
 * Copyright 2013 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VBOOT_CROSSYSTEM_CROSSYSTEM_H__
#define __VBOOT_CROSSYSTEM_CROSSYSTEM_H__

struct vboot_info *vboot;

/**
 * enum cros_fw_type_t - typet of firmware we can selected to boot
 *
 * The values must match host/lib/include/crossystem_arch.h BINF3_*.
 * Pass FIRMWARE_TYPE_AUTO_DETECT to crossystem_setup to detect and select
 * from one of the types: (recovery, normal, developer).
 */
enum cros_fw_type_t {
	FIRMWARE_TYPE_AUTO_DETECT = -1,
	FIRMWARE_TYPE_RECOVERY = 0,
	FIRMWARE_TYPE_NORMAL = 1,
	FIRMWARE_TYPE_DEVELOPER = 2,
	FIRMWARE_TYPE_NETBOOT = 3,
	FIRMWARE_TYPE_LEGACY = 4,
};

// Setup the crossystem data. This should be done as late as possible to
// ensure the data used is up to date.
int crossystem_setup(struct vboot_info *vboot,
		     enum cros_fw_type_t firmware_type);

/**
 * vboot_update_acpi() - Update ACPI data
 *
 * For x86 systems, this writes a basic level of information in binary to
 * the ACPI tables for use by the kernel.
 *
 * It also updates the SMBIOS type 0 version string with the firmware ID of the
 * firmware being booted.
 *
 * This uses the BLOBLISTT_ACPI_GNVS blob in the bloblist.
 *
 * When booting frem coreboot, bloblist is not available. In that case it uses
 * the sysinfo acpi_gnvs pointer to find the correct place to update.
 *
 * @vboot: Pointer to vboot structure
 * @fw_type: Firmware type to boot
 * @return 0 if OK, -ve on error
 */
int vboot_update_acpi(struct vboot_info *vboot, enum cros_fw_type_t fw_type);

#endif /* __VBOOT_CROSSYSTEM_CROSSYSTEM_H__ */
