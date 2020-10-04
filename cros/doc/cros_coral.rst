.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2020 Google LLC

Vboot on coral
==============

This provides a few details about running vboot on coral, a range of
Intel-based Chromebooks launched in 2017. See :doc:`../../arch/x86` for more
information on x86.

This uses a slightly later version of vboot (late 2018) from that which the
product launched with, but the changes are minor.


Build and run
-------------

This unfortunately needs binary blobs to work. See
:doc:`../../board/google/chromebook_coral` for more information.

Also quite a few data files use by vboot are required. These can be extracted
from an image as described above, but to save you the trouble, they are
included in the git tree here.

To obtain::

   git clone https://github.com/sjg20/u-boot.git
   cd u-boot
   git checkout cros-2021.01

   cd ..
   git clone https://chromium.googlesource.com/chromiumos/platform/vboot_reference
   cd vboot_reference
   git checkout 45964294
   #  futility: updater: Correct output version for Snow

To build for coral::

   UB=/tmp/b/chromeos_coral    # U-Boot build directory
   cd u-boot
   make O=$UB chromeos_coral_defconfig
   make O=$UB -j20 -s VBOOT_SOURCE=/path/to/vboot_reference \
     MAKEFLAGS_VBOOT=DEBUG=1 QUIET=1

That should produce `/tmp/b/chromos_coral/image.bin` which you can use with
a Dediprog em100::

  em100 -s -c w25q128fw -d /tmp/b/chromeos_coral/image.bin -r

Boot flow
---------

See :doc:`../../board/google/chromebook_coral` for details on the standard
Coral boot flow. This section describes only what is different with vboot.

With verified boot an extra 'VPL' (Verifying Program Loader) phase is inserted
after TPL. This runs most of the vboot steps, including selecting the correct
SPL image to boot.

SPL itself doesn't do any vboot things, so, like TPL, it is basically the same
as without vboot. It just sets up the SDRAM.

When U-Boot proper starts up, it runs the vboot UI via the 'vboot run auto'
command. However the device boots up in normal mode by default, so you won't
actually see anything on the display. It will simply boot straight to Chrome OS.

There are 'nvdata' and 'secdata' commands that let you adjust the vboot
settings, but bear in mind that the TPM is partially locked by the time you get
to the U-Boot command line. You can also boot to recovery mode and change it
there (Esc-Refresh-Power).


Sample run
----------

This shows the output from a sample run, booting into normal mode::

  U-Boot TPL 2021.04-rc1-00128-g344eefcdfec-dirty (Feb 11 2021 - 20:48:13 -0700)
  Trying to boot from Mapped SPI

  U-Boot VPL 2021.04-rc1-00128-g344eefcdfec-dirty (Feb 11 2021 - 20:48:13 -0700)
  Trying to boot from chromium_vboot_vpl
  Running stage 'ver_init'
  Vboot nvdata:
     Signature v1, size 16 (valid), CRC 5a (calc 5a, valid)
     - kernel settings reset
     - firmware settings reset
     - backup nvram
     Result 0, prev 0
     Recovery 0, subcode 0
     Localization 0, default boot 0, kernel 0, max roll-forward 0
  Vboot secdata:
  00000000: 02 00 01 00 01 00 00 00 00 4f                      .........O
     Size 10 : valid
     CRC 4f (calc 4f): valid
     Version 2
     Firmware versions 10001
  Running stage 'ver1_vbinit'
  GBB: Reading SPI flash offset=202000, size=80
  vb2_check_recovery: Recovery reason from previous boot: 0x0 / 0x0
  Running stage 'ver2_selectfw'
  Running stage 'ver3_tryfw'
  GBB: Reading SPI flash offset=202180, size=1000
  vb2_report_dev_firmware: This is developer signed firmware
  Slot A: Reading SPI flash offset=4b0000, size=70
  Slot A: Reading SPI flash offset=4b0000, size=8b8
  vb2_verify_keyblock: Checking key block signature...
  Slot A: Reading SPI flash offset=4b08b8, size=6c
  Slot A: Reading SPI flash offset=4b08b8, size=874
  vb2_verify_fw_preamble: Verifying preamble.
  Running stage 'ver4_locatefw'
  Setting up firmware reader at 4b2000, size 157da
  Hashing firmware body, expected size 157da
  vb2api_init_hash: HW crypto for hash_alg 2 not supported, using SW
  is_resume=0
  write type 4 size 40
  Running stage 'ver5_finishfw'
  Slot A is selected
  Creating vboot_handoff structure
  Copying FW preamble
  flags 0 recovery=0, EC=cros-ec
  Running stage 'ver_jump'
  Reading firmware offset 4b2000 (addr fef11000, size 157da)
  Ready to jump to firmware
  Completed loading image

  U-Boot SPL 2021.04-rc1-00128-g344eefcdfec-dirty (Feb 11 2021 - 20:48:13 -0700)
  Trying to boot from chromium_vboot_spl
  Running stage 'spl_init'
  Running stage 'spl_jump_u_boot'
  Reading firmware offset 4c8000 (addr 1110000, size adb14)
  Completed loading image


  U-Boot 2021.04-rc1-00128-g344eefcdfec-dirty (Feb 11 2021 - 20:48:13 -0700)

  CPU:   Intel(R) Celeron(R) CPU N3450 @ 1.10GHz
  DRAM:  3.9 GiB
  MMC:   sdmmc@1b,0: 1, emmc@1c,0: 2
  Video: 1024x768x32 @ b0000000
  Model: Google Coral
  Net:   No ethernet found.
  SF: Detected w25q128fw with page size 256 Bytes, erase size 4 KiB, total 16 MiB
  Hit any key to stop autoboot:  0
  Running stage 'rw_init'
  flags 0 0
  Found shared_data_blob at 799080fc, size 3072
  Running stage 'rw_selectkernel'
  tpm_get_response: command 0x14e, return code 0x0
  RollbackKernelRead: TPM: RollbackKernelRead 10001
  tpm_get_response: command 0x14e, return code 0x28b
  RollbackFwmpRead: TPM: no FWMP space
  print_hash: RW(active) hash: 8071ddc08f62784f4ee6629f5968a9ce47d6c8a94e85681a2acf0c8f6da07f64
  sync_one_ec: devidx=0 select_rw=4
  sync_one_ec: jumping to EC-RW
  VbBootNormal: Entering
  VbTryLoadKernel: VbTryLoadKernel() start, get_info_flags=0x2
  sdhci_send_command: Timeout for status update!
  Found 1 disks
  VbTryLoadKernel: VbTryLoadKernel() found 1 disks
  VbTryLoadKernel: VbTryLoadKernel() trying disk 0
  GptNextKernelEntry: GptNextKernelEntry looking at new prio partition 2
  GptNextKernelEntry: GptNextKernelEntry s1 t0 p1
  GptNextKernelEntry: GptNextKernelEntry looking at new prio partition 4
  GptNextKernelEntry: GptNextKernelEntry s0 t0 p0
  GptNextKernelEntry: GptNextKernelEntry looking at new prio partition 6
  GptNextKernelEntry: GptNextKernelEntry s0 t15 p0
  GptNextKernelEntry: GptNextKernelEntry likes partition 2
  LoadKernel: Found kernel entry at 20480 size 32768
  vb2_verify_keyblock: Checking key block signature...
  vb2_verify_kernel_preamble: Verifying kernel preamble.
  vb2_verify_kernel_vblock: Kernel preamble is good.
  vb2_load_partition: Partition is good.
  LoadKernel: Key block valid: 1
  LoadKernel: Combined version: 65537
  LoadKernel: Same kernel version
  LoadKernel: Good partition 2
  VbTryLoadKernel: VbTryLoadKernel() LoadKernel() = 0
  VbBootNormal: Checking if TPM kernel version needs advancing
  tpm_get_response: command 0x121, return code 0x0
  VbSelectAndLoadKernel: Returning 0
  Running stage 'rw_bootkernel'
  partition_number=2, guid=35c775e7-3735-d745-93e5-d9e0238f7ed0
  Bloblist:
  Address       Size  Tag Name
  79908030        b0    3 Chrome OS vboot context
  799080f0       c0c    4 Chrome OS vboot hand-off
  79908d10        90    2 SPL hand-off
  79909000     10000    9 ACPI tables for x86
  79919010      1000    5 ACPI GNVS
  7991a020     10000    7 TPM v2 log space
  7992a030      180a    6 Intel Video-BIOS table
  7992b900      1000   10 SMBIOS tables for x86
  Kernel command line: "cros_secure  console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 root=PARTUUID=35c775e7-3735-d745-93e5-d9e0238f7ed0/PARTNROFF=1 rootwait rw dm_verity.error_behavior=3 dm_verity.max_bios=-1 dm_verity.dev_wait=0 dm="1 vroot none rw 1,0 3788800 verity payload=ROOT_DEV hashtree=HASH_DEV hashstart=3788800 alg=sha1 root_hexdigest=55052b629d3ac889f25a9583ea12cdcd3ea15ff8 salt=a2d4d9e574069f4fed5e3961b99054b7a4905414b60a25d89974a7334021165c" noinitrd vt.global_cursor_default=0 kern_guid=35c775e7-3735-d745-93e5-d9e0238f7ed0 add_efi_memmap boot=local noresume noswap i915.modeset=1 tpm_tis.force=1 tpm_tis.interrupts=0 nmi_watchdog=panic,lapic disablevmx=off  "

  Starting kernel ...

  Timer summary in microseconds (37 records):
         Mark    Elapsed  Stage
            0          0
      155,241    155,241
      269,229    113,988
      269,573        344  VPL
      286,073     16,500  ver_init
      393,258    107,185  user_1
      462,446     69,188  user_2
      482,568     20,122  ver3_tryfw
      574,322     91,754  user_4
      582,053      7,731  user_5
      629,135     47,082  user_7
      639,355     10,220  user_6
      646,129      6,774  user_8
      768,147    122,018  user_9
      768,151          4  user_10
      826,149     57,998  user_11
      894,430     68,281  user_13
      940,402     45,972  end phase
      940,454         52  SPL
    1,638,790    698,336  end phase
    1,639,590        800  board_init_f
    1,974,190    334,600  board_init_r
    2,324,819    350,629  id=64
    2,374,808     49,989  main_loop
    2,739,799    364,991  user_12
    5,219,594  2,479,795  user_14
    5,381,751    162,157  start_kernel

  Accumulated time:
                     952  dm_r
                  32,374  user_17
                  58,057  dm_spl
                  70,393  dm_f
                 168,141  mmap_spi
                 209,192  fsp-m
                 241,286  fsp-s
                 354,289  fast_spi
               1,066,419  boot_device_read
               1,114,692  boot_device_info

