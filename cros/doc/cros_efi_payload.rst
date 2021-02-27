.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2020 Google LLC

Vboot as an EFI payload
=======================

This provides a few details about running vboot on top of EFI, useful for
boards which have an existing UEFI BIOS. See
:doc:`../../develop/uefi/u-boot_on_efi` for more information on the EFI payload.

This uses the latest version of vboot, with a few patches to make it more
U-Boot-friendly.


Build and run
-------------

To obtain::

   git clone https://github.com/sjg20/u-boot.git
   cd u-boot
   git checkout cros-working

   cd ..
   git clone https://chromium.googlesource.com/chromiumos/platform/vboot_reference
   cd vboot_reference
   git checkout origin/working
   #  Revert "Avoid using malloc() directly"

To build for efi::

   UB=/tmp/b/chromeos_efi-x86_payload/    # U-Boot build directory
   cd u-boot
   make O=$UB chromeos_efi-x86_payload_defconfig
   make O=$UB -j20 -s VBOOT_SOURCE=/path/to/vboot_reference \
     MAKEFLAGS_VBOOT=DEBUG=1 QUIET=1

To run on EFI put the resulting `u-boot-payload.efi` file in a qemu image.


Boot flow
---------

This is still to be determined. It starts vboot at the 'read-write init'
stage which means that it just needs to boot a kernel.

At present this builds but doesn't do anything useful when run. The required
boot flow needs to be figured out.


Exanple boot flow
-----------------

This is minimal so far. It immediately fails in vboot_rw_init() because there
is no bloblist. Nor is there any coreboot. So we need to work out what state to
set up and this will be a new case in `rw_init.c`::

   U-Boot 2021.07-00106-g3c3721560eb-dirty (Jul 25 2021 - 20:29:04 -0600)

   CPU: x86_64, vendor AMD, device 663h
   DRAM:  84.1 MiB
   MMC:
   Loading Environment from nowhere... OK
   Video: 800x600x32
   Model: EFI x86 Payload
   Net:   No ethernet found.
   No working controllers found
   Hit any key to stop autoboot:  0
   --
   * Running stage 'rw_init'
   Chromium OS verified boot starting
   blob: returning err=-2
   Error: stage 'rw_init' returned fffffffe (-2)
   Cold reboot
