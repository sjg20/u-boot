.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2021 Google LLC

Vboot on coreboot
=================

This provides a few details about running vboot on coreboot, currently only
tested on coral, a range of Intel-based Chromebooks launched in 2017. See
:doc:`../../arch/x86` for more information on x86.

This uses a slightly later version of vboot (late 2018) from that which the
product launched with, but the changes are minor.


Build and run
-------------

Quite a few data files use by vboot are required. These can be extracted
from an image as described above, but to save you the trouble, they are
included in the git tree here. However you still need the ROM image to actually
boot.

To obtain sources::

   git clone https://github.com/sjg20/u-boot.git
   cd u-boot
   git checkout cros-2021.04# Args:

   cd ..
   git clone https://chromium.googlesource.com/chromiumos/platform/vboot_reference
   cd vboot_reference
   git checkout 45964294
   #  futility: updater: Correct output version for Snow

To build for chromeos_coreboot::

   UB=/tmp/b/chromeos_coreboot    # U-Boot build directory
   cd u-boot
   make O=$UB chromeos_coreboot_defconfig
   make O=$UB -j20 -s VBOOT_SOURCE=/path/to/vboot_reference \
     MAKEFLAGS_VBOOT=DEBUG=1 QUIET=1

That should produce `/tmp/b/chromeos_coreboot/u-boot.bin` which you can then
insert into the Chromium OS image::

  # Put old ROM image at ~/image-coral.bin
  # Run this script to produce cb.bin
  cros/doc/replace_fallback.sh

  # Write to the board
  em100 -s -c w25q128fw -d cb.bin -r


Boot flow
---------

The full boot flow is specific to coreboot, but once vboot is completed,
coreboot starts U-Boot.

When U-Boot proper starts up, it runs the vboot UI via the 'vboot run auto'
command. However the device boots up in normal mode by default, so you won't
actually see anything on the display. It will simply boot straight to Chrome OS.

There is an 'nvdata' and 'secdata' commands that let you adjust the vboot
settings, but bear in mind that the TPM is locked by the time you get to the
U-Boot command line. You can also boot to recovery mode and change it
there (Esc-Refresh-Power).


Sample run
----------

This shows the output from a sample run, booting into normal mode. Scroll to
near the end to see the U-Boot output.

::

  coreboot-v1.9308_26_0.0.22-2599-g232f22c75d Wed Nov 18 02:51:58 UTC 2020 bootblock starting...
  LPC: Trying to open IO window from 800 size 1ff
  LPC: Opened IO window LGIR0: base 800 size 100
  LPC: Opened IO window LGIR1: base 900 size 100
  VBOOT: Loading verstage.
  FMAP: Found "FLASH" version 1.1 at 204000.
  FMAP: base = 0 size = 1000000 #areas = 35
  FMAP: area COREBOOT found @ 205000 (1552384 bytes)
  CBFS @ 205000 size 17b000
  CBFS: 'IAFW Locator' located CBFS at [205000:380000)
  CBFS: Locating 'fallback/verstage'
  CBFS: Found @ offset ba4c0 size fcd4


  coreboot-v1.9308_26_0.0.22-2599-g232f22c75d Wed Nov 18 02:51:58 UTC 2020 verstage starting...
  LPSS I2C bus 2 at 0xfe022000 (400 KHz)
  TPM ready after 4 ms
  cr50 TPM 2.0 (i2c 2:0x50 id 0x28)
  TPM Command: 0x00000144
  fef09140: 80 01 00 00 00 0c 00 00 01 44 00 00              .........D..
  TPM Response: 0x00000000
  fef09140: 80 01 00 00 00 0a 00 00 00 00                    ..........
  setup_tpm():514: TPM: SetupTPM() succeeded
  TPM Command: 0x0000014e
  fef09140: 80 02 00 00 00 23 00 00 01 4e 40 00 00 0c 01 00  .....#...N@.....
  fef09150: 10 07 00 00 00 09 40 00 00 09 00 00 00 00 00 00  ......@.........
  fef09160: 0a 00 00                                         ...
  TPM Response: 0x00000000
  fef09140: 80 02 00 00 00 1f 00 00 00 00 00 00 00 0c 00 0a  ................
  fef09150: 02 01 01 00 01 00 00 00 00 5c 00 00 01 00 00     .........\.....
  src/lib/tpm2_tlcl.c:208 index 0x1007 return code 0
  Phase 1
  FMAP: Found "FLASH" version 1.1 at 204000.
  FMAP: base = 0 size = 1000000 #areas = 35
  FMAP: area GBB found @ 380000 (262144 bytes)
  VB2:vb2_check_recovery() Recovery reason from previous boot: 0x0 / 0x0
  Phase 2
  Phase 3
  FMAP: area GBB found @ 380000 (262144 bytes)
  VB2:vb2_report_dev_firmware() This is developer signed firmware
  FMAP: area VBLOCK_A found @ 430000 (65536 bytes)
  FMAP: area VBLOCK_A found @ 430000 (65536 bytes)
  VB2:vb2_verify_keyblock() Checking key block signature...
  FMAP: area VBLOCK_A found @ 430000 (65536 bytes)
  FMAP: area VBLOCK_A found @ 430000 (65536 bytes)
  VB2:vb2_verify_fw_preamble() Verifying preamble.
  Phase 4
  FMAP: area FW_MAIN_A found @ 440000 (4652992 bytes)
  VB2:vb2api_init_hash() HW crypto for hash_alg 2 not supported, using SW
  Saving vboot hash.
  TPM Command: 0x00000182
  fef09140: 80 02 00 00 00 41 00 00 01 82 00 00 00 00 00 00  .....A..........
  fef09150: 00 09 40 00 00 09 00 00 00 00 00 00 00 00 01 00  ..@.............
  fef09160: 0b c4 2a c1 c4 6f 1d 4e 21 1c 73 5c c7 df ad 4f  ..*..o.N!.s\...O
  fef09170: f8 39 11 10 e9 00 00 00 00 00 00 00 00 00 00 00  .9..............
  fef09180: 00                                               .
  TPM Response: 0x00000000
  fef09140: 80 02 00 00 00 13 00 00 00 00 00 00 00 00 00 00  ................
  fef09150: 01 00 00                                         ...
  tlcl_extend: response is 0
  TPM Command: 0x00000182
  fef09140: 80 02 00 00 00 41 00 00 01 82 00 00 00 01 00 00  .....A..........
  fef09150: 00 09 40 00 00 09 00 00 00 00 00 00 00 00 01 00  ..@.............
  fef09160: 0b 83 f9 cf 40 2d 25 c0 80 b8 1c c3 7b da 5c ae  ....@-%.....{.\.
  fef09170: 81 74 12 63 07 8c 57 47 8e a1 6e f1 fb ca 72 8b  .t.c..WG..n...r.
  fef09180: e8                                               .
  TPM Response: 0x00000000
  fef09140: 80 02 00 00 00 13 00 00 00 00 00 00 00 00 00 00  ................
  fef09150: 01 00 00                                         ...
  tlcl_extend: response is 0
  TPM Command: 0x00000138
  fef09140: 80 02 00 00 00 1f 00 00 01 38 40 00 00 0c 01 00  .........8@.....
  fef09150: 10 07 00 00 00 09 40 00 00 09 00 00 00 00 00     ......@........
  TPM Response: 0x00000000
  fef09140: 80 02 00 00 00 13 00 00 00 00 00 00 00 00 00 00  ................
  fef09150: 01 00 00                                         ...
  tlcl_lock_nv_write: response is 0
  TPM Command: 0x00000138
  fef09140: 80 02 00 00 00 1f 00 00 01 38 40 00 00 0c 01 00  .........8@.....
  fef09150: 10 0b 00 00 00 09 40 00 00 09 00 00 00 00 00     ......@........
  TPM Response: 0x00000000
  fef09140: 80 02 00 00 00 13 00 00 00 00 00 00 00 00 00 00  ................
  fef09150: 01 00 00                                         ...
  tlcl_lock_nv_write: response is 0
  Slot A is selected
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fallback/romstage'
  CBFS: Found @ offset 0 size 11e9c


  coreboot-v1.9308_26_0.0.22-2599-g232f22c75d Wed Nov 18 02:51:58 UTC 2020 romstage starting...
  pm1_sts: 0000 pm1_en: 0000 pm1_cnt: 00001c00
  gpe0_sts[0]: 00000000 gpe0_en[0]: 00000000
  gpe0_sts[1]: 00000000 gpe0_en[1]: 00000000
  gpe0_sts[2]: 00000000 gpe0_en[2]: 00000000
  gpe0_sts[3]: 00000000 gpe0_en[3]: 00000000
  prsts: 00000000 tco_sts: 00000000
  gen_pmcon1: 08004000 gen_pmcon2: 00003a00 gen_pmcon3: 00000000
  prev_sleep_state 0
  Boot Count incremented to 28
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fspm.bin'
  CBFS: Found @ offset 2f2c0 size 59000
  FMAP: Found "FLASH" version 1.1 at 204000.
  FMAP: base = 0 size = 1000000 #areas = 35
  FMAP: area RW_MRC_CACHE found @ 410000 (65536 bytes)
  MRC cache found, size b2c0 bootmode:2
  LPDDR4 SKU id = 0x5
  LP4DDR speed is 2400MHz
  LPDDR4 Ch0 density = 2
  LPDDR4 Ch1 density = 2
  FMAP: area RW_VAR_MRC_CACHE found @ 420000 (4096 bytes)
  CBMEM:
  IMD: root @ 7afff000 254 entries.
  IMD: root @ 7affec00 62 entries.
  External stage cache:
  IMD: root @ 7b7ff000 254 entries.
  IMD: root @ 7b7fec00 62 entries.
  creating vboot_handoff structure
  Copying FW preamble
  Chrome EC: clear events_b mask to 0x21004000
  CPU: frequency set to 2200 MHz
  4 DIMMs found
  MTRR Range: Start=7a000000 End=7b000000 (Size 1000000)
  MTRR Range: Start=ff000000 End=0 (Size 1000000)
  MTRR Range: Start=7b000000 End=7b800000 (Size 800000)
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fallback/postcar'
  CBFS: Found @ offset b3000 size 45b8
  Decompressing stage fallback/postcar @ 0x7abc5fc0 (34440 bytes)
  Loading module at 7abc6000 with entry 7abc6000. filesize: 0x4350 memsize: 0x8648
  Processing 131 relocs. Offset value of 0x78bc6000


  coreboot-v1.9308_26_0.0.22-2599-g232f22c75d Wed Nov 18 02:51:58 UTC 2020 postcar starting...
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fallback/ramstage'
  CBFS: Found @ offset 15f80 size 17d1a
  Decompressing stage fallback/ramstage @ 0x7ab77fc0 (314352 bytes)
  Loading module at 7ab78000 with entry 7ab78000. filesize: 0x34818 memsize: 0x4cbb0
  Processing 3002 relocs. Offset value of 0x7aa78000


  coreboot-v1.9308_26_0.0.22-2599-g232f22c75d Wed Nov 18 02:51:58 UTC 2020 ramstage starting...
  FMAP: Found "FLASH" version 1.1 at 204000.
  FMAP: base = 0 size = 1000000 #areas = 35
  FMAP: area RO_VPD found @ 200000 (16384 bytes)
  WARNING: RO_VPD is uninitialized or empty.
  FMAP: area RW_VPD found @ 428000 (8192 bytes)
  WARNING: RW_VPD is uninitialized or empty.
  Normal boot.
  BS: BS_PRE_DEVICE times (us): entry 2 run 33 exit 0
  Board ID: 2
  mainboard: EC init
  Chrome EC: Set WAKE mask to 0x00000000
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fsps.bin'
  CBFS: Found @ offset 88fc0 size 2a000
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'vbt-santa.bin'
  CBFS: Found @ offset d0400 size 521
  Found a VBT of 6656 bytes after decompression
  ITSS IRQ Polarities Before:
  IPC0: 0xffffeef8
  IPC1: 0xffffffff
  IPC2: 0xffffffff
  IPC3: 0x00ffffff
  ITSS IRQ Polarities After:
  IPC0: 0xffffeef8
  IPC1: 0x4a07ffff
  IPC2: 0x08000000
  IPC3: 0x00a11000
  RAPL PL1 12.0W
  RAPL PL2 15.0W
  BS: BS_DEV_INIT_CHIPS times (us): entry 0 run 322936 exit 0
  Enumerating buses...
  Show all devs... Before device enumeration.
  Root Device: enabled 1
  CPU_CLUSTER: 0: enabled 1
  APIC: 00: enabled 1
  DOMAIN: 0000: enabled 1
  PCI: 00:00.0: enabled 1
  PCI: 00:00.1: enabled 1
  PCI: 00:00.2: enabled 1
  PCI: 00:02.0: enabled 1
  PCI: 00:03.0: enabled 1
  PCI: 00:0d.0: enabled 1
  PCI: 00:0d.1: enabled 1
  PCI: 00:0d.2: enabled 1
  PCI: 00:0d.3: enabled 1
  PCI: 00:0e.0: enabled 1
  GENERIC: 0.0: enabled 1
  PCI: 00:11.0: enabled 0
  PCI: 00:12.0: enabled 0
  PCI: 00:13.0: enabled 0
  PCI: 00:13.1: enabled 0
  PCI: 00:13.2: enabled 0
  PCI: 00:13.3: enabled 0
  PCI: 00:14.0: enabled 1
  PCI: 00:00.0: enabled 1
  PCI: 00:14.1: enabled 0
  PCI: 00:15.0: enabled 1
  PCI: 00:15.1: enabled 0
  PCI: 00:16.0: enabled 1
  I2C: 00:1a: enabled 1
  PCI: 00:16.1: enabled 1
  PCI: 00:16.2: enabled 1
  I2C: 00:50: enabled 1
  PCI: 00:16.3: enabled 1
  I2C: 00:10: enabled 1
  I2C: 00:39: enabled 1
  PCI: 00:17.0: enabled 1
  I2C: 00:15: enabled 1
  I2C: 00:2c: enabled 1
  PCI: 00:17.1: enabled 1
  I2C: 00:09: enabled 1
  PCI: 00:17.2: enabled 0
  PCI: 00:17.3: enabled 0
  PCI: 00:18.0: enabled 1
  PCI: 00:18.1: enabled 1
  PCI: 00:18.2: enabled 1
  PCI: 00:18.3: enabled 0
  PCI: 00:19.0: enabled 1
  PCI: 00:19.1: enabled 0
  PCI: 00:19.2: enabled 0
  PCI: 00:1a.0: enabled 1
  PCI: 00:1b.0: enabled 1
  PCI: 00:1c.0: enabled 1
  PCI: 00:1e.0: enabled 0
  PCI: 00:1f.0: enabled 1
  PNP: 0c09.0: enabled 1
  PCI: 00:1f.1: enabled 1
  Compare with tree...
  Root Device: enabled 1
   CPU_CLUSTER: 0: enabled 1
    APIC: 00: enabled 1
   DOMAIN: 0000: enabled 1
    PCI: 00:00.0: enabled 1
    PCI: 00:00.1: enabled 1
    PCI: 00:00.2: enabled 1
    PCI: 00:02.0: enabled 1
    PCI: 00:03.0: enabled 1
    PCI: 00:0d.0: enabled 1
    PCI: 00:0d.1: enabled 1
    PCI: 00:0d.2: enabled 1
    PCI: 00:0d.3: enabled 1
    PCI: 00:0e.0: enabled 1
     GENERIC: 0.0: enabled 1
    PCI: 00:11.0: enabled 0
    PCI: 00:12.0: enabled 0
    PCI: 00:13.0: enabled 0
    PCI: 00:13.1: enabled 0
    PCI: 00:13.2: enabled 0
    PCI: 00:13.3: enabled 0
    PCI: 00:14.0: enabled 1
     PCI: 00:00.0: enabled 1
    PCI: 00:14.1: enabled 0
    PCI: 00:15.0: enabled 1
    PCI: 00:15.1: enabled 0
    PCI: 00:16.0: enabled 1
     I2C: 00:1a: enabled 1
    PCI: 00:16.1: enabled 1
    PCI: 00:16.2: enabled 1
     I2C: 00:50: enabled 1
    PCI: 00:16.3: enabled 1
     I2C: 00:10: enabled 1
     I2C: 00:39: enabled 1
    PCI: 00:17.0: enabled 1
     I2C: 00:15: enabled 1
     I2C: 00:2c: enabled 1
    PCI: 00:17.1: enabled 1
     I2C: 00:09: enabled 1
    PCI: 00:17.2: enabled 0
    PCI: 00:17.3: enabled 0
    PCI: 00:18.0: enabled 1
    PCI: 00:18.1: enabled 1
    PCI: 00:18.2: enabled 1
    PCI: 00:18.3: enabled 0
    PCI: 00:19.0: enabled 1
    PCI: 00:19.1: enabled 0
    PCI: 00:19.2: enabled 0
    PCI: 00:1a.0: enabled 1
    PCI: 00:1b.0: enabled 1
    PCI: 00:1c.0: enabled 1
    PCI: 00:1e.0: enabled 0
    PCI: 00:1f.0: enabled 1
     PNP: 0c09.0: enabled 1
    PCI: 00:1f.1: enabled 1
  Root Device scanning...
  root_dev_scan_bus for Root Device
  CPU_CLUSTER: 0 enabled
  DOMAIN: 0000 enabled
  DOMAIN: 0000 scanning...
  PCI: pci_scan_bus for bus 00
  PCI: 00:00.0 [8086/0000] ops
  PCI: 00:00.0 [8086/5af0] enabled
  PCI: 00:00.1 [8086/5a8c] enabled
  PCI: 00:00.2 [8086/5a8e] enabled
  PCI: 00:02.0 [8086/0000] ops
  PCI: 00:02.0 [8086/5a85] enabled
  PCI: 00:03.0 [8086/5a88] enabled
  PCI: 00:0d.0 [8086/0000] ops
  PCI: 00:0d.0 [8086/5a92] enabled
  PCI: 00:0d.1 [8086/0000] ops
  PCI: 00:0d.1 [8086/5a94] enabled
  PCI: 00:0d.2 [8086/5a96] enabled
  PCI: 00:0d.3 [8086/0000] ops
  PCI: 00:0d.3 [8086/5aec] enabled
  PCI: 00:0e.0 [8086/0000] bus ops
  PCI: 00:0e.0 [8086/5a98] enabled
  PCI: 00:0f.0 [8086/0000] ops
  PCI: 00:0f.0 [8086/5a9a] enabled
  PCI: 00:0f.1 [8086/5a9c] enabled
  PCI: 00:0f.2 [8086/5a9e] enabled
  Capability: type 0x10 @ 0x40
  Capability: type 0x05 @ 0x80
  Capability: type 0x0d @ 0x90
  Capability: type 0x01 @ 0xa0
  Capability: type 0x10 @ 0x40
  PCI: 00:14.0 subordinate bus PCI Express
  PCI: 00:14.0 [8086/5ad6] enabled
  PCI: 00:15.0 [8086/0000] ops
  PCI: 00:15.0 [8086/5aa8] enabled
  PCI: 00:16.0 [8086/0000] bus ops
  PCI: 00:16.0 [8086/5aac] enabled
  PCI: 00:16.1 [8086/0000] bus ops
  PCI: 00:16.1 [8086/5aae] enabled
  PCI: 00:16.2 [8086/0000] bus ops
  PCI: 00:16.2 [8086/5ab0] enabled
  PCI: 00:16.3 [8086/0000] bus ops
  PCI: 00:16.3 [8086/5ab2] enabled
  PCI: 00:17.0 [8086/0000] bus ops
  PCI: 00:17.0 [8086/5ab4] enabled
  PCI: 00:17.1 [8086/0000] bus ops
  PCI: 00:17.1 [8086/5ab6] enabled
  PCI: 00:18.0 [8086/0000] ops
  PCI: 00:18.0 [8086/5abc] enabled
  PCI: 00:18.1 [8086/0000] ops
  PCI: 00:18.1 [8086/5abe] enabled
  PCI: 00:18.2 [8086/0000] ops
  PCI: 00:18.2 [8086/5ac0] enabled
  PCI: 00:19.0 [8086/5ac2] enabled
  PCI: Static device PCI: 00:1a.0 not found, disabling it.
  PCI: 00:1b.0 [8086/0000] ops
  PCI: 00:1b.0 [8086/5aca] enabled
  PCI: 00:1c.0 [8086/5acc] enabled
  PCI: 00:1f.0 [8086/0000] bus ops
  PCI: 00:1f.0 [8086/5ae8] enabled
  PCI: 00:1f.1 [8086/5ad4] enabled
  PCI: 00:0e.0 scanning...
  GENERIC: 0.0 enabled
  scan_bus: scanning of bus PCI: 00:0e.0 took 4850 usecs
  PCI: 00:14.0 scanning...
  do_pci_scan_bridge for PCI: 00:14.0
  PCI: pci_scan_bus for bus 01
  PCI: 01:00.0 [8086/095a] enabled
  Capability: type 0x01 @ 0xc8
  Capability: type 0x05 @ 0xd0
  Capability: type 0x10 @ 0x40
  Capability: type 0x10 @ 0x40
  Enabling Common Clock Configuration
  L1 Sub-State supported from root port 20
  L1 Sub-State Support = 0xf
  CommonModeRestoreTime = 0x28
  Power On Value = 0x1e, Power On Scale = 0x0
  ASPM: Enabled L1
  Capability: type 0x01 @ 0xc8
  Capability: type 0x05 @ 0xd0
  Capability: type 0x10 @ 0x40
  scan_bus: scanning of bus PCI: 00:14.0 took 52899 usecs
  PCI: 00:16.0 scanning...
  scan_generic_bus for PCI: 00:16.0
  bus: PCI: 00:16.0[0]->I2C: 01:1a enabled
  scan_generic_bus for PCI: 00:16.0 done
  scan_bus: scanning of bus PCI: 00:16.0 took 14098 usecs
  PCI: 00:16.1 scanning...
  scan_generic_bus for PCI: 00:16.1
  scan_generic_bus for PCI: 00:16.1 done
  scan_bus: scanning of bus PCI: 00:16.1 took 10001 usecs
  PCI: 00:16.2 scanning...
  scan_generic_bus for PCI: 00:16.2
  bus: PCI: 00:16.2[0]->I2C: 02:50 enabled
  scan_generic_bus for PCI: 00:16.2 done
  scan_bus: scanning of bus PCI: 00:16.2 took 14111 usecs
  PCI: 00:16.3 scanning...
  scan_generic_bus for PCI: 00:16.3
  bus: PCI: 00:16.3[0]->I2C: 03:10 enabled
  bus: PCI: 00:16.3[0]->I2C: 03:39 enabled
  scan_generic_bus for PCI: 00:16.3 done
  scan_bus: scanning of bus PCI: 00:16.3 took 18180 usecs
  PCI: 00:17.0 scanning...
  scan_generic_bus for PCI: 00:17.0
  bus: PCI: 00:17.0[0]->I2C: 04:15 enabled
  bus: PCI: 00:17.0[0]->I2C: 04:2c enabled
  scan_generic_bus for PCI: 00:17.0 done
  scan_bus: scanning of bus PCI: 00:17.0 took 18185 usecs
  PCI: 00:17.1 scanning...
  scan_generic_bus for PCI: 00:17.1
  bus: PCI: 00:17.1[0]->I2C: 05:09 enabled
  scan_generic_bus for PCI: 00:17.1 done
  scan_bus: scanning of bus PCI: 00:17.1 took 14112 usecs
  PCI: 00:1f.0 scanning...
  scan_lpc_bus for PCI: 00:1f.0
  PNP: 0c09.0 enabled
  scan_lpc_bus for PCI: 00:1f.0 done
  scan_bus: scanning of bus PCI: 00:1f.0 took 11280 usecs
  scan_bus: scanning of bus DOMAIN: 0000 took 392347 usecs
  root_dev_scan_bus for Root Device done
  scan_bus: scanning of bus Root Device took 412371 usecs
  done
  FMAP: area RW_MRC_CACHE found @ 410000 (65536 bytes)
  MRC: Checking cached data update for 'RW_MRC_CACHE'.
  SF: Detected FAST_SPI Hardware Sequencer with sector size 0x1000, total 0x1000000
  FMAP: area RW_VAR_MRC_CACHE found @ 420000 (4096 bytes)
  MRC: Checking cached data update for 'RW_VAR_MRC_CACHE'.
  MRC: cache data 'RW_VAR_MRC_CACHE' needs update.
  FMAP: area RW_ELOG found @ 421000 (12288 bytes)
  ELOG: NV offset 0x421000 size 0x3000
  ELOG: NV Buffer Cleared.
  ELOG: Event(16) added with size 11 at 2020-11-19 20:35:58 UTC
  ELOG: area is 4096 bytes, full threshold 3842, shrink size 1024
  ELOG: Event(17) added with size 13 at 2020-11-19 20:35:58 UTC
  ELOG: Event(AA) added with size 11 at 2020-11-19 20:35:58 UTC
  FMAP: area UNIFIED_MRC_CACHE found @ 400000 (135168 bytes)
  SPI flash protection: WPSW=1 SRP0=0
  MRC: NOT enabling PRR for 'UNIFIED_MRC_CACHE'.
  BS: BS_DEV_ENUMERATE times (us): entry 0 run 702552 exit 92532
  found VGA at PCI: 00:02.0
  Setting up VGA for PCI: 00:02.0
  Setting PCI_BRIDGE_CTL_VGA for bridge DOMAIN: 0000
  Setting PCI_BRIDGE_CTL_VGA for bridge Root Device
  Allocating resources...
  Reading resources...
  Root Device read_resources bus 0 link: 0
  CPU_CLUSTER: 0 read_resources bus 0 link: 0
  CPU_CLUSTER: 0 read_resources bus 0 link: 0 done
  DOMAIN: 0000 read_resources bus 0 link: 0
  PCI: 00:0e.0 read_resources bus 0 link: 0
  PCI: 00:0e.0 read_resources bus 0 link: 0 done
  PCI: 00:14.0 read_resources bus 1 link: 0
  PCI: 00:14.0 read_resources bus 1 link: 0 done
  PCI: 00:16.0 read_resources bus 1 link: 0
  PCI: 00:16.0 read_resources bus 1 link: 0 done
  PCI: 00:16.2 read_resources bus 2 link: 0
  PCI: 00:16.2 read_resources bus 2 link: 0 done
  PCI: 00:16.3 read_resources bus 3 link: 0
  PCI: 00:16.3 read_resources bus 3 link: 0 done
  PCI: 00:17.0 read_resources bus 4 link: 0
  PCI: 00:17.0 read_resources bus 4 link: 0 done
  PCI: 00:17.1 read_resources bus 5 link: 0
  PCI: 00:17.1 read_resources bus 5 link: 0 done
  PCI: 00:1f.0 read_resources bus 0 link: 0
  PCI: 00:1f.0 read_resources bus 0 link: 0 done
  DOMAIN: 0000 read_resources bus 0 link: 0 done
  Root Device read_resources bus 0 link: 0 done
  Done reading resources.
  Show resources in subtree (Root Device)...After reading.
   Root Device child on link 0 CPU_CLUSTER: 0
    CPU_CLUSTER: 0 child on link 0 APIC: 00
     APIC: 00
    DOMAIN: 0000 child on link 0 PCI: 00:00.0
    DOMAIN: 0000 resource base 0 size 0 align 0 gran 0 limit ffff flags 40040100 index 10000000
    DOMAIN: 0000 resource base 0 size 0 align 0 gran 0 limit ffffffff flags 40040200 index 10000100
     PCI: 00:00.0
     PCI: 00:00.0 resource base e0000000 size 10000000 align 0 gran 0 limit 0 flags f0000200 index 0
     PCI: 00:00.0 resource base fed10000 size 8000 align 0 gran 0 limit 0 flags f0000200 index 1
     PCI: 00:00.0 resource base 0 size a0000 align 0 gran 0 limit 0 flags e0004200 index 2
     PCI: 00:00.0 resource base c0000 size 7af40000 align 0 gran 0 limit 0 flags e0004200 index 3
     PCI: 00:00.0 resource base 7b000000 size 800000 align 0 gran 0 limit 0 flags f0004200 index 5
     PCI: 00:00.0 resource base 7b800000 size 4800000 align 0 gran 0 limit 0 flags f0000200 index 6
     PCI: 00:00.0 resource base 100000000 size 80000000 align 0 gran 0 limit 0 flags e0004200 index 7
     PCI: 00:00.0 resource base a0000 size 20000 align 0 gran 0 limit 0 flags f0000200 index 8
     PCI: 00:00.0 resource base c0000 size 40000 align 0 gran 0 limit 0 flags f0004200 index 9
     PCI: 00:00.0 resource base 11800000 size 400000 align 0 gran 0 limit 0 flags f0004200 index a
     PCI: 00:00.0 resource base 11000000 size 800000 align 0 gran 0 limit 0 flags f0004200 index b
     PCI: 00:00.0 resource base 12000000 size 100000 align 0 gran 0 limit 0 flags f0004200 index c
     PCI: 00:00.0 resource base 12150000 size 1000 align 0 gran 0 limit 0 flags f0004200 index d
     PCI: 00:00.0 resource base 12140000 size 10000 align 0 gran 0 limit 0 flags f0004200 index e
     PCI: 00:00.0 resource base 10000000 size 1000000 align 0 gran 0 limit 0 flags f0004200 index f
     PCI: 00:00.0 resource base 11c00000 size 400000 align 0 gran 0 limit 0 flags f0004200 index 10
     PCI: 00:00.0 resource base 12100000 size 40000 align 0 gran 0 limit 0 flags f0004200 index 11
     PCI: 00:00.1
     PCI: 00:00.1 resource base 0 size 8000 align 15 gran 15 limit ffffffffffffffff flags 201 index 10
     PCI: 00:00.2
     PCI: 00:00.2 resource base 0 size 100000 align 20 gran 20 limit ffffffffffffffff flags 201 index 10
     PCI: 00:00.2 resource base 0 size 800000 align 23 gran 23 limit ffffffffffffffff flags 201 index 18
     PCI: 00:00.2 resource base 0 size 200 align 12 gran 9 limit ffffffffffffffff flags 201 index 20
     PCI: 00:02.0
     PCI: 00:02.0 resource base 0 size 1000000 align 24 gran 24 limit ffffffffffffffff flags 201 index 10
     PCI: 00:02.0 resource base 0 size 10000000 align 28 gran 28 limit ffffffffffffffff flags 1201 index 18
     PCI: 00:02.0 resource base 0 size 40 align 6 gran 6 limit ffff flags 100 index 20
     PCI: 00:03.0
     PCI: 00:03.0 resource base 0 size 1000000 align 24 gran 24 limit 7fffffffff flags 201 index 10
     PCI: 00:0d.0
     PCI: 00:0d.0 resource base d0000000 size 1000000 align 0 gran 0 limit 0 flags f0000200 index 10
     PCI: 00:0d.1
     PCI: 00:0d.1 resource base fe042000 size 2000 align 0 gran 0 limit 0 flags c0000200 index 10
     PCI: 00:0d.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:0d.1 resource base 400 size 100 align 0 gran 0 limit 0 flags c0000100 index 20
     PCI: 00:0d.2
     PCI: 00:0d.2 resource base 0 size 1000 align 12 gran 12 limit ffffffff flags 200 index 10
     PCI: 00:0d.3
     PCI: 00:0d.3 resource base fe900000 size 2000 align 0 gran 0 limit 0 flags c0000200 index 10
     PCI: 00:0d.3 resource base fe902000 size 1000 align 0 gran 0 limit 0 flags c0000200 index 18
     PCI: 00:0e.0 child on link 0 GENERIC: 0.0
     PCI: 00:0e.0 resource base 0 size 4000 align 14 gran 14 limit ffffffffffffffff flags 201 index 10
     PCI: 00:0e.0 resource base 0 size 100000 align 20 gran 20 limit ffffffffffffffff flags 201 index 20
      GENERIC: 0.0
     PCI: 00:0f.0
     PCI: 00:0f.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:0f.1
     PCI: 00:0f.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:0f.2
     PCI: 00:0f.2 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:11.0
     PCI: 00:12.0
     PCI: 00:13.0
     PCI: 00:13.1
     PCI: 00:13.2
     PCI: 00:13.3
     PCI: 00:14.0 child on link 0 PCI: 01:00.0
     PCI: 00:14.0 resource base 0 size 0 align 12 gran 12 limit ffff flags 80102 index 1c
     PCI: 00:14.0 resource base 0 size 0 align 20 gran 20 limit ffffffffffffffff flags 81202 index 24
     PCI: 00:14.0 resource base 0 size 0 align 20 gran 20 limit ffffffff flags 80202 index 20
      PCI: 01:00.0
      PCI: 01:00.0 resource base 0 size 2000 align 13 gran 13 limit ffffffffffffffff flags 201 index 10
     PCI: 00:14.1
     PCI: 00:15.0
     PCI: 00:15.0 resource base 0 size 10000 align 16 gran 16 limit ffffffffffffffff flags 201 index 10
     PCI: 00:15.1
     PCI: 00:16.0 child on link 0 I2C: 01:1a
     PCI: 00:16.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:16.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
      I2C: 01:1a
     PCI: 00:16.1
     PCI: 00:16.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:16.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:16.2 child on link 0 I2C: 02:50
     PCI: 00:16.2 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:16.2 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
      I2C: 02:50
     PCI: 00:16.3 child on link 0 I2C: 03:10
     PCI: 00:16.3 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:16.3 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
      I2C: 03:10
      I2C: 03:39
     PCI: 00:17.0 child on link 0 I2C: 04:15
     PCI: 00:17.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:17.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
      I2C: 04:15
      I2C: 04:2c
     PCI: 00:17.1 child on link 0 I2C: 05:09
     PCI: 00:17.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:17.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
      I2C: 05:09
     PCI: 00:17.2
     PCI: 00:17.3
     PCI: 00:18.0
     PCI: 00:18.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:18.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:18.1
     PCI: 00:18.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:18.1 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:18.2
     PCI: 00:18.2 resource base de000000 size 1000 align 0 gran 0 limit 0 flags e0000200 index 10
     PCI: 00:18.2 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:18.3
     PCI: 00:19.0
     PCI: 00:19.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:19.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:19.1
     PCI: 00:19.2
     PCI: 00:1a.0
     PCI: 00:1b.0
     PCI: 00:1b.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:1b.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:1c.0
     PCI: 00:1c.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 10
     PCI: 00:1c.0 resource base 0 size 1000 align 12 gran 12 limit ffffffffffffffff flags 201 index 18
     PCI: 00:1e.0
     PCI: 00:1f.0 child on link 0 PNP: 0c09.0
     PCI: 00:1f.0 resource base 0 size 1000 align 0 gran 0 limit 0 flags c0000100 index 0
      PNP: 0c09.0
      PNP: 0c09.0 resource base 800 size 1ff align 0 gran 0 limit 0 flags c0000100 index 0
     PCI: 00:1f.1
     PCI: 00:1f.1 resource base 0 size 100 align 12 gran 8 limit ffffffffffffffff flags 201 index 10
     PCI: 00:1f.1 resource base 0 size 20 align 5 gran 5 limit ffff flags 100 index 20
  DOMAIN: 0000 io: base: 0 size: 0 align: 0 gran: 0 limit: ffff
  PCI: 00:14.0 io: base: 0 size: 0 align: 12 gran: 12 limit: ffff
  PCI: 00:14.0 io: base: 0 size: 0 align: 12 gran: 12 limit: ffff done
  PCI: 00:02.0 20 *  [0x0 - 0x3f] io
  PCI: 00:1f.1 20 *  [0x40 - 0x5f] io
  DOMAIN: 0000 io: base: 60 size: 60 align: 6 gran: 0 limit: ffff done
  DOMAIN: 0000 mem: base: 0 size: 0 align: 0 gran: 0 limit: ffffffff
  PCI: 00:14.0 prefmem: base: 0 size: 0 align: 20 gran: 20 limit: ffffffffffffffff
  PCI: 00:14.0 prefmem: base: 0 size: 0 align: 20 gran: 20 limit: ffffffffffffffff done
  PCI: 00:14.0 mem: base: 0 size: 0 align: 20 gran: 20 limit: ffffffff
  PCI: 01:00.0 10 *  [0x0 - 0x1fff] mem
  PCI: 00:14.0 mem: base: 2000 size: 100000 align: 20 gran: 20 limit: ffffffff done
  PCI: 00:02.0 18 *  [0x0 - 0xfffffff] prefmem
  PCI: 00:02.0 10 *  [0x10000000 - 0x10ffffff] mem
  PCI: 00:03.0 10 *  [0x11000000 - 0x11ffffff] mem
  PCI: 00:00.2 18 *  [0x12000000 - 0x127fffff] mem
  PCI: 00:00.2 10 *  [0x12800000 - 0x128fffff] mem
  PCI: 00:0e.0 20 *  [0x12900000 - 0x129fffff] mem
  PCI: 00:14.0 20 *  [0x12a00000 - 0x12afffff] mem
  PCI: 00:15.0 10 *  [0x12b00000 - 0x12b0ffff] mem
  PCI: 00:00.1 10 *  [0x12b10000 - 0x12b17fff] mem
  PCI: 00:0e.0 10 *  [0x12b18000 - 0x12b1bfff] mem
  PCI: 00:0d.1 18 *  [0x12b1c000 - 0x12b1cfff] mem
  PCI: 00:0d.2 10 *  [0x12b1d000 - 0x12b1dfff] mem
  PCI: 00:0f.0 10 *  [0x12b1e000 - 0x12b1efff] mem
  PCI: 00:0f.1 10 *  [0x12b1f000 - 0x12b1ffff] mem
  PCI: 00:0f.2 10 *  [0x12b20000 - 0x12b20fff] mem
  PCI: 00:16.0 10 *  [0x12b21000 - 0x12b21fff] mem
  PCI: 00:16.0 18 *  [0x12b22000 - 0x12b22fff] mem
  PCI: 00:16.1 10 *  [0x12b23000 - 0x12b23fff] mem
  PCI: 00:16.1 18 *  [0x12b24000 - 0x12b24fff] mem
  PCI: 00:16.2 10 *  [0x12b25000 - 0x12b25fff] mem
  PCI: 00:16.2 18 *  [0x12b26000 - 0x12b26fff] mem
  PCI: 00:16.3 10 *  [0x12b27000 - 0x12b27fff] mem
  PCI: 00:16.3 18 *  [0x12b28000 - 0x12b28fff] mem
  PCI: 00:17.0 10 *  [0x12b29000 - 0x12b29fff] mem
  PCI: 00:17.0 18 *  [0x12b2a000 - 0x12b2afff] mem
  PCI: 00:17.1 10 *  [0x12b2b000 - 0x12b2bfff] mem
  PCI: 00:17.1 18 *  [0x12b2c000 - 0x12b2cfff] mem
  PCI: 00:18.0 10 *  [0x12b2d000 - 0x12b2dfff] mem
  PCI: 00:18.0 18 *  [0x12b2e000 - 0x12b2efff] mem
  PCI: 00:18.1 10 *  [0x12b2f000 - 0x12b2ffff] mem
  PCI: 00:18.1 18 *  [0x12b30000 - 0x12b30fff] mem
  PCI: 00:18.2 18 *  [0x12b31000 - 0x12b31fff] mem
  PCI: 00:19.0 10 *  [0x12b32000 - 0x12b32fff] mem
  PCI: 00:19.0 18 *  [0x12b33000 - 0x12b33fff] mem
  PCI: 00:1b.0 10 *  [0x12b34000 - 0x12b34fff] mem
  PCI: 00:1b.0 18 *  [0x12b35000 - 0x12b35fff] mem
  PCI: 00:1c.0 10 *  [0x12b36000 - 0x12b36fff] mem
  PCI: 00:1c.0 18 *  [0x12b37000 - 0x12b37fff] mem
  PCI: 00:00.2 20 *  [0x12b38000 - 0x12b381ff] mem
  PCI: 00:1f.1 10 *  [0x12b39000 - 0x12b390ff] mem
  DOMAIN: 0000 mem: base: 12b39100 size: 12b39100 align: 28 gran: 0 limit: ffffffff done
  avoid_fixed_resources: DOMAIN: 0000
  avoid_fixed_resources:@DOMAIN: 0000 10000000 limit 0000ffff
  avoid_fixed_resources:@DOMAIN: 0000 10000100 limit ffffffff
  constrain_resources: PCI: 00:00.0 00 base e0000000 limit efffffff mem (fixed)
  constrain_resources: PCI: 00:00.0 02 base 00000000 limit 0009ffff mem (fixed)
  constrain_resources: PCI: 00:00.0 03 base 000c0000 limit 7affffff mem (fixed)
  constrain_resources: PCI: 00:00.0 05 base 7b000000 limit 7b7fffff mem (fixed)
  constrain_resources: PCI: 00:00.0 06 base 7b800000 limit 7fffffff mem (fixed)
  constrain_resources: PCI: 00:0d.0 10 base d0000000 limit d0ffffff mem (fixed)
  constrain_resources: PCI: 00:0d.1 20 base 00000400 limit 000004ff io (fixed)
  constrain_resources: PCI: 00:1f.0 00 base 00000000 limit 00000fff io (fixed)
  avoid_fixed_resources:@DOMAIN: 0000 10000000 base 00001000 limit 0000ffff
  avoid_fixed_resources:@DOMAIN: 0000 10000100 base b0000000 limit cfffffff
  Setting resources...
  DOMAIN: 0000 io: base:1000 size:60 align:6 gran:0 limit:ffff
  PCI: 00:02.0 20 *  [0x1000 - 0x103f] io
  PCI: 00:1f.1 20 *  [0x1040 - 0x105f] io
  DOMAIN: 0000 io: next_base: 1060 size: 60 align: 6 gran: 0 done
  PCI: 00:14.0 io: base:ffff size:0 align:12 gran:12 limit:ffff
  PCI: 00:14.0 io: next_base: ffff size: 0 align: 12 gran: 12 done
  DOMAIN: 0000 mem: base:b0000000 size:12b39100 align:28 gran:0 limit:cfffffff
  PCI: 00:02.0 18 *  [0xb0000000 - 0xbfffffff] prefmem
  PCI: 00:02.0 10 *  [0xc0000000 - 0xc0ffffff] mem
  PCI: 00:03.0 10 *  [0xc1000000 - 0xc1ffffff] mem
  PCI: 00:00.2 18 *  [0xc2000000 - 0xc27fffff] mem
  PCI: 00:00.2 10 *  [0xc2800000 - 0xc28fffff] mem
  PCI: 00:0e.0 20 *  [0xc2900000 - 0xc29fffff] mem
  PCI: 00:14.0 20 *  [0xc2a00000 - 0xc2afffff] mem
  PCI: 00:15.0 10 *  [0xc2b00000 - 0xc2b0ffff] mem
  PCI: 00:00.1 10 *  [0xc2b10000 - 0xc2b17fff] mem
  PCI: 00:0e.0 10 *  [0xc2b18000 - 0xc2b1bfff] mem
  PCI: 00:0d.1 18 *  [0xc2b1c000 - 0xc2b1cfff] mem
  PCI: 00:0d.2 10 *  [0xc2b1d000 - 0xc2b1dfff] mem
  PCI: 00:0f.0 10 *  [0xc2b1e000 - 0xc2b1efff] mem
  PCI: 00:0f.1 10 *  [0xc2b1f000 - 0xc2b1ffff] mem
  PCI: 00:0f.2 10 *  [0xc2b20000 - 0xc2b20fff] mem
  PCI: 00:16.0 10 *  [0xc2b21000 - 0xc2b21fff] mem
  PCI: 00:16.0 18 *  [0xc2b22000 - 0xc2b22fff] mem
  PCI: 00:16.1 10 *  [0xc2b23000 - 0xc2b23fff] mem
  PCI: 00:16.1 18 *  [0xc2b24000 - 0xc2b24fff] mem
  PCI: 00:16.2 10 *  [0xc2b25000 - 0xc2b25fff] mem
  PCI: 00:16.2 18 *  [0xc2b26000 - 0xc2b26fff] mem
  PCI: 00:16.3 10 *  [0xc2b27000 - 0xc2b27fff] mem
  PCI: 00:16.3 18 *  [0xc2b28000 - 0xc2b28fff] mem
  PCI: 00:17.0 10 *  [0xc2b29000 - 0xc2b29fff] mem
  PCI: 00:17.0 18 *  [0xc2b2a000 - 0xc2b2afff] mem
  PCI: 00:17.1 10 *  [0xc2b2b000 - 0xc2b2bfff] mem
  PCI: 00:17.1 18 *  [0xc2b2c000 - 0xc2b2cfff] mem
  PCI: 00:18.0 10 *  [0xc2b2d000 - 0xc2b2dfff] mem
  PCI: 00:18.0 18 *  [0xc2b2e000 - 0xc2b2efff] mem
  PCI: 00:18.1 10 *  [0xc2b2f000 - 0xc2b2ffff] mem
  PCI: 00:18.1 18 *  [0xc2b30000 - 0xc2b30fff] mem
  PCI: 00:18.2 18 *  [0xc2b31000 - 0xc2b31fff] mem
  PCI: 00:19.0 10 *  [0xc2b32000 - 0xc2b32fff] mem
  PCI: 00:19.0 18 *  [0xc2b33000 - 0xc2b33fff] mem
  PCI: 00:1b.0 10 *  [0xc2b34000 - 0xc2b34fff] mem
  PCI: 00:1b.0 18 *  [0xc2b35000 - 0xc2b35fff] mem
  PCI: 00:1c.0 10 *  [0xc2b36000 - 0xc2b36fff] mem
  PCI: 00:1c.0 18 *  [0xc2b37000 - 0xc2b37fff] mem
  PCI: 00:00.2 20 *  [0xc2b38000 - 0xc2b381ff] mem
  PCI: 00:1f.1 10 *  [0xc2b39000 - 0xc2b390ff] mem
  DOMAIN: 0000 mem: next_base: c2b39100 size: 12b39100 align: 28 gran: 0 done
  PCI: 00:14.0 prefmem: base:cfffffff size:0 align:20 gran:20 limit:cfffffff
  PCI: 00:14.0 prefmem: next_base: cfffffff size: 0 align: 20 gran: 20 done
  PCI: 00:14.0 mem: base:c2a00000 size:100000 align:20 gran:20 limit:c2afffff
  PCI: 01:00.0 10 *  [0xc2a00000 - 0xc2a01fff] mem
  PCI: 00:14.0 mem: next_base: c2a02000 size: 100000 align: 20 gran: 20 done
  Root Device assign_resources, bus 0 link: 0
  DOMAIN: 0000 assign_resources, bus 0 link: 0
  PCI: 00:00.1 10 <- [0x00c2b10000 - 0x00c2b17fff] size 0x00008000 gran 0x0f mem64
  PCI: 00:00.2 10 <- [0x00c2800000 - 0x00c28fffff] size 0x00100000 gran 0x14 mem64
  PCI: 00:00.2 18 <- [0x00c2000000 - 0x00c27fffff] size 0x00800000 gran 0x17 mem64
  PCI: 00:00.2 20 <- [0x00c2b38000 - 0x00c2b381ff] size 0x00000200 gran 0x09 mem64
  PCI: 00:02.0 10 <- [0x00c0000000 - 0x00c0ffffff] size 0x01000000 gran 0x18 mem64
  PCI: 00:02.0 18 <- [0x00b0000000 - 0x00bfffffff] size 0x10000000 gran 0x1c prefmem64
  PCI: 00:02.0 20 <- [0x0000001000 - 0x000000103f] size 0x00000040 gran 0x06 io
  PCI: 00:03.0 10 <- [0x00c1000000 - 0x00c1ffffff] size 0x01000000 gran 0x18 mem64
  PCI: 00:0d.1 18 <- [0x00c2b1c000 - 0x00c2b1cfff] size 0x00001000 gran 0x0c mem64
  PCI: 00:0d.1 10 <- [0x00fe042000 - 0x00fe043fff] size 0x00002000 gran 0x00 mem PMC BAR
  PCI: 00:0d.1 20 <- [0x0000000400 - 0x00000004ff] size 0x00000100 gran 0x00 io ACPI BAR
  PCI: 00:0d.2 10 <- [0x00c2b1d000 - 0x00c2b1dfff] size 0x00001000 gran 0x0c mem
  PCI: 00:0d.3 10 <- [0x00fe900000 - 0x00fe901fff] size 0x00002000 gran 0x00 mem SRAM BAR 0
  PCI: 00:0d.3 18 <- [0x00fe902000 - 0x00fe902fff] size 0x00001000 gran 0x00 mem SRAM BAR 2
  PCI: 00:0e.0 10 <- [0x00c2b18000 - 0x00c2b1bfff] size 0x00004000 gran 0x0e mem64
  PCI: 00:0e.0 20 <- [0x00c2900000 - 0x00c29fffff] size 0x00100000 gran 0x14 mem64
  PCI: 00:0e.0 assign_resources, bus 0 link: 0
  PCI: 00:0e.0 assign_resources, bus 0 link: 0
  PCI: 00:0f.0 10 <- [0x00c2b1e000 - 0x00c2b1efff] size 0x00001000 gran 0x0c mem64
  PCI: 00:0f.1 10 <- [0x00c2b1f000 - 0x00c2b1ffff] size 0x00001000 gran 0x0c mem64
  PCI: 00:0f.2 10 <- [0x00c2b20000 - 0x00c2b20fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:14.0 1c <- [0x000000ffff - 0x000000fffe] size 0x00000000 gran 0x0c bus 01 io
  PCI: 00:14.0 24 <- [0x00cfffffff - 0x00cffffffe] size 0x00000000 gran 0x14 bus 01 prefmem
  PCI: 00:14.0 20 <- [0x00c2a00000 - 0x00c2afffff] size 0x00100000 gran 0x14 bus 01 mem
  PCI: 00:14.0 assign_resources, bus 1 link: 0
  PCI: 01:00.0 10 <- [0x00c2a00000 - 0x00c2a01fff] size 0x00002000 gran 0x0d mem64
  PCI: 00:14.0 assign_resources, bus 1 link: 0
  PCI: 00:15.0 10 <- [0x00c2b00000 - 0x00c2b0ffff] size 0x00010000 gran 0x10 mem64
  PCI: 00:16.0 10 <- [0x00c2b21000 - 0x00c2b21fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.0 18 <- [0x00c2b22000 - 0x00c2b22fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.0 assign_resources, bus 1 link: 0
  PCI: 00:16.0 assign_resources, bus 1 link: 0
  PCI: 00:16.1 10 <- [0x00c2b23000 - 0x00c2b23fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.1 18 <- [0x00c2b24000 - 0x00c2b24fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.2 10 <- [0x00c2b25000 - 0x00c2b25fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.2 18 <- [0x00c2b26000 - 0x00c2b26fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.2 assign_resources, bus 2 link: 0
  PCI: 00:16.2 assign_resources, bus 2 link: 0
  PCI: 00:16.3 10 <- [0x00c2b27000 - 0x00c2b27fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.3 18 <- [0x00c2b28000 - 0x00c2b28fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:16.3 assign_resources, bus 3 link: 0
  PCI: 00:16.3 assign_resources, bus 3 link: 0
  PCI: 00:17.0 10 <- [0x00c2b29000 - 0x00c2b29fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:17.0 18 <- [0x00c2b2a000 - 0x00c2b2afff] size 0x00001000 gran 0x0c mem64
  PCI: 00:17.0 assign_resources, bus 4 link: 0
  PCI: 00:17.0 assign_resources, bus 4 link: 0
  PCI: 00:17.1 10 <- [0x00c2b2b000 - 0x00c2b2bfff] size 0x00001000 gran 0x0c mem64
  PCI: 00:17.1 18 <- [0x00c2b2c000 - 0x00c2b2cfff] size 0x00001000 gran 0x0c mem64
  PCI: 00:17.1 assign_resources, bus 5 link: 0
  PCI: 00:17.1 assign_resources, bus 5 link: 0
  PCI: 00:18.0 10 <- [0x00c2b2d000 - 0x00c2b2dfff] size 0x00001000 gran 0x0c mem64
  PCI: 00:18.0 18 <- [0x00c2b2e000 - 0x00c2b2efff] size 0x00001000 gran 0x0c mem64
  PCI: 00:18.1 10 <- [0x00c2b2f000 - 0x00c2b2ffff] size 0x00001000 gran 0x0c mem64
  PCI: 00:18.1 18 <- [0x00c2b30000 - 0x00c2b30fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:18.2 18 <- [0x00c2b31000 - 0x00c2b31fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:19.0 10 <- [0x00c2b32000 - 0x00c2b32fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:19.0 18 <- [0x00c2b33000 - 0x00c2b33fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:1b.0 10 <- [0x00c2b34000 - 0x00c2b34fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:1b.0 18 <- [0x00c2b35000 - 0x00c2b35fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:1c.0 10 <- [0x00c2b36000 - 0x00c2b36fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:1c.0 18 <- [0x00c2b37000 - 0x00c2b37fff] size 0x00001000 gran 0x0c mem64
  PCI: 00:1f.0 assign_resources, bus 0 link: 0
  PNP: 0c09.0 missing set_resources
  PCI: 00:1f.0 assign_resources, bus 0 link: 0
  LPC: Trying to open IO window from 800 size 1ff
  LPC: Opened IO window LGIR3: base 800 size 100
  LPC: Cannot open IO window: 900 size ff
  No more IO windows
  PCI: 00:1f.1 10 <- [0x00c2b39000 - 0x00c2b390ff] size 0x00000100 gran 0x08 mem64
  PCI: 00:1f.1 20 <- [0x0000001040 - 0x000000105f] size 0x00000020 gran 0x05 io
  DOMAIN: 0000 assign_resources, bus 0 link: 0
  Root Device assign_resources, bus 0 link: 0
  Done setting resources.
  Show resources in subtree (Root Device)...After assigning values.
   Root Device child on link 0 CPU_CLUSTER: 0
    CPU_CLUSTER: 0 child on link 0 APIC: 00
     APIC: 00
    DOMAIN: 0000 child on link 0 PCI: 00:00.0
    DOMAIN: 0000 resource base 1000 size 60 align 6 gran 0 limit ffff flags 40040100 index 10000000
    DOMAIN: 0000 resource base b0000000 size 12b39100 align 28 gran 0 limit cfffffff flags 40040200 index 10000100
     PCI: 00:00.0
     PCI: 00:00.0 resource base e0000000 size 10000000 align 0 gran 0 limit 0 flags f0000200 index 0
     PCI: 00:00.0 resource base fed10000 size 8000 align 0 gran 0 limit 0 flags f0000200 index 1
     PCI: 00:00.0 resource base 0 size a0000 align 0 gran 0 limit 0 flags e0004200 index 2
     PCI: 00:00.0 resource base c0000 size 7af40000 align 0 gran 0 limit 0 flags e0004200 index 3
     PCI: 00:00.0 resource base 7b000000 size 800000 align 0 gran 0 limit 0 flags f0004200 index 5
     PCI: 00:00.0 resource base 7b800000 size 4800000 align 0 gran 0 limit 0 flags f0000200 index 6
     PCI: 00:00.0 resource base 100000000 size 80000000 align 0 gran 0 limit 0 flags e0004200 index 7
     PCI: 00:00.0 resource base a0000 size 20000 align 0 gran 0 limit 0 flags f0000200 index 8
     PCI: 00:00.0 resource base c0000 size 40000 align 0 gran 0 limit 0 flags f0004200 index 9
     PCI: 00:00.0 resource base 11800000 size 400000 align 0 gran 0 limit 0 flags f0004200 index a
     PCI: 00:00.0 resource base 11000000 size 800000 align 0 gran 0 limit 0 flags f0004200 index b
     PCI: 00:00.0 resource base 12000000 size 100000 align 0 gran 0 limit 0 flags f0004200 index c
     PCI: 00:00.0 resource base 12150000 size 1000 align 0 gran 0 limit 0 flags f0004200 index d
     PCI: 00:00.0 resource base 12140000 size 10000 align 0 gran 0 limit 0 flags f0004200 index e
     PCI: 00:00.0 resource base 10000000 size 1000000 align 0 gran 0 limit 0 flags f0004200 index f
     PCI: 00:00.0 resource base 11c00000 size 400000 align 0 gran 0 limit 0 flags f0004200 index 10
     PCI: 00:00.0 resource base 12100000 size 40000 align 0 gran 0 limit 0 flags f0004200 index 11
     PCI: 00:00.1
     PCI: 00:00.1 resource base c2b10000 size 8000 align 15 gran 15 limit c2b17fff flags 60000201 index 10
     PCI: 00:00.2
     PCI: 00:00.2 resource base c2800000 size 100000 align 20 gran 20 limit c28fffff flags 60000201 index 10
     PCI: 00:00.2 resource base c2000000 size 800000 align 23 gran 23 limit c27fffff flags 60000201 index 18
     PCI: 00:00.2 resource base c2b38000 size 200 align 12 gran 9 limit c2b381ff flags 60000201 index 20
     PCI: 00:02.0
     PCI: 00:02.0 resource base c0000000 size 1000000 align 24 gran 24 limit c0ffffff flags 60000201 index 10
     PCI: 00:02.0 resource base b0000000 size 10000000 align 28 gran 28 limit bfffffff flags 60001201 index 18
     PCI: 00:02.0 resource base 1000 size 40 align 6 gran 6 limit 103f flags 60000100 index 20
     PCI: 00:03.0
     PCI: 00:03.0 resource base c1000000 size 1000000 align 24 gran 24 limit c1ffffff flags 60000201 index 10
     PCI: 00:0d.0
     PCI: 00:0d.0 resource base d0000000 size 1000000 align 0 gran 0 limit 0 flags f0000200 index 10
     PCI: 00:0d.1
     PCI: 00:0d.1 resource base fe042000 size 2000 align 0 gran 0 limit 0 flags e0000200 index 10
     PCI: 00:0d.1 resource base c2b1c000 size 1000 align 12 gran 12 limit c2b1cfff flags 60000201 index 18
     PCI: 00:0d.1 resource base 400 size 100 align 0 gran 0 limit 0 flags e0000100 index 20
     PCI: 00:0d.2
     PCI: 00:0d.2 resource base c2b1d000 size 1000 align 12 gran 12 limit c2b1dfff flags 60000200 index 10
     PCI: 00:0d.3
     PCI: 00:0d.3 resource base fe900000 size 2000 align 0 gran 0 limit 0 flags e0000200 index 10
     PCI: 00:0d.3 resource base fe902000 size 1000 align 0 gran 0 limit 0 flags e0000200 index 18
     PCI: 00:0e.0 child on link 0 GENERIC: 0.0
     PCI: 00:0e.0 resource base c2b18000 size 4000 align 14 gran 14 limit c2b1bfff flags 60000201 index 10
     PCI: 00:0e.0 resource base c2900000 size 100000 align 20 gran 20 limit c29fffff flags 60000201 index 20
      GENERIC: 0.0
     PCI: 00:0f.0
     PCI: 00:0f.0 resource base c2b1e000 size 1000 align 12 gran 12 limit c2b1efff flags 60000201 index 10
     PCI: 00:0f.1
     PCI: 00:0f.1 resource base c2b1f000 size 1000 align 12 gran 12 limit c2b1ffff flags 60000201 index 10
     PCI: 00:0f.2
     PCI: 00:0f.2 resource base c2b20000 size 1000 align 12 gran 12 limit c2b20fff flags 60000201 index 10
     PCI: 00:11.0
     PCI: 00:12.0
     PCI: 00:13.0
     PCI: 00:13.1
     PCI: 00:13.2
     PCI: 00:13.3
     PCI: 00:14.0 child on link 0 PCI: 01:00.0
     PCI: 00:14.0 resource base ffff size 0 align 12 gran 12 limit ffff flags 60080102 index 1c
     PCI: 00:14.0 resource base cfffffff size 0 align 20 gran 20 limit cfffffff flags 60081202 index 24
     PCI: 00:14.0 resource base c2a00000 size 100000 align 20 gran 20 limit c2afffff flags 60080202 index 20
      PCI: 01:00.0
      PCI: 01:00.0 resource base c2a00000 size 2000 align 13 gran 13 limit c2a01fff flags 60000201 index 10
     PCI: 00:14.1
     PCI: 00:15.0
     PCI: 00:15.0 resource base c2b00000 size 10000 align 16 gran 16 limit c2b0ffff flags 60000201 index 10
     PCI: 00:15.1
     PCI: 00:16.0 child on link 0 I2C: 01:1a
     PCI: 00:16.0 resource base c2b21000 size 1000 align 12 gran 12 limit c2b21fff flags 60000201 index 10
     PCI: 00:16.0 resource base c2b22000 size 1000 align 12 gran 12 limit c2b22fff flags 60000201 index 18
      I2C: 01:1a
     PCI: 00:16.1
     PCI: 00:16.1 resource base c2b23000 size 1000 align 12 gran 12 limit c2b23fff flags 60000201 index 10
     PCI: 00:16.1 resource base c2b24000 size 1000 align 12 gran 12 limit c2b24fff flags 60000201 index 18
     PCI: 00:16.2 child on link 0 I2C: 02:50
     PCI: 00:16.2 resource base c2b25000 size 1000 align 12 gran 12 limit c2b25fff flags 60000201 index 10
     PCI: 00:16.2 resource base c2b26000 size 1000 align 12 gran 12 limit c2b26fff flags 60000201 index 18
      I2C: 02:50
     PCI: 00:16.3 child on link 0 I2C: 03:10
     PCI: 00:16.3 resource base c2b27000 size 1000 align 12 gran 12 limit c2b27fff flags 60000201 index 10
     PCI: 00:16.3 resource base c2b28000 size 1000 align 12 gran 12 limit c2b28fff flags 60000201 index 18
      I2C: 03:10
      I2C: 03:39
     PCI: 00:17.0 child on link 0 I2C: 04:15
     PCI: 00:17.0 resource base c2b29000 size 1000 align 12 gran 12 limit c2b29fff flags 60000201 index 10
     PCI: 00:17.0 resource base c2b2a000 size 1000 align 12 gran 12 limit c2b2afff flags 60000201 index 18
      I2C: 04:15
      I2C: 04:2c
     PCI: 00:17.1 child on link 0 I2C: 05:09
     PCI: 00:17.1 resource base c2b2b000 size 1000 align 12 gran 12 limit c2b2bfff flags 60000201 index 10
     PCI: 00:17.1 resource base c2b2c000 size 1000 align 12 gran 12 limit c2b2cfff flags 60000201 index 18
      I2C: 05:09
     PCI: 00:17.2
     PCI: 00:17.3
     PCI: 00:18.0
     PCI: 00:18.0 resource base c2b2d000 size 1000 align 12 gran 12 limit c2b2dfff flags 60000201 index 10
     PCI: 00:18.0 resource base c2b2e000 size 1000 align 12 gran 12 limit c2b2efff flags 60000201 index 18
     PCI: 00:18.1
     PCI: 00:18.1 resource base c2b2f000 size 1000 align 12 gran 12 limit c2b2ffff flags 60000201 index 10
     PCI: 00:18.1 resource base c2b30000 size 1000 align 12 gran 12 limit c2b30fff flags 60000201 index 18
     PCI: 00:18.2
     PCI: 00:18.2 resource base de000000 size 1000 align 0 gran 0 limit 0 flags e0000200 index 10
     PCI: 00:18.2 resource base c2b31000 size 1000 align 12 gran 12 limit c2b31fff flags 60000201 index 18
     PCI: 00:18.3
     PCI: 00:19.0
     PCI: 00:19.0 resource base c2b32000 size 1000 align 12 gran 12 limit c2b32fff flags 60000201 index 10
     PCI: 00:19.0 resource base c2b33000 size 1000 align 12 gran 12 limit c2b33fff flags 60000201 index 18
     PCI: 00:19.1
     PCI: 00:19.2
     PCI: 00:1a.0
     PCI: 00:1b.0
     PCI: 00:1b.0 resource base c2b34000 size 1000 align 12 gran 12 limit c2b34fff flags 60000201 index 10
     PCI: 00:1b.0 resource base c2b35000 size 1000 align 12 gran 12 limit c2b35fff flags 60000201 index 18
     PCI: 00:1c.0
     PCI: 00:1c.0 resource base c2b36000 size 1000 align 12 gran 12 limit c2b36fff flags 60000201 index 10
     PCI: 00:1c.0 resource base c2b37000 size 1000 align 12 gran 12 limit c2b37fff flags 60000201 index 18
     PCI: 00:1e.0
     PCI: 00:1f.0 child on link 0 PNP: 0c09.0
     PCI: 00:1f.0 resource base 0 size 1000 align 0 gran 0 limit 0 flags c0000100 index 0
      PNP: 0c09.0
      PNP: 0c09.0 resource base 800 size 1ff align 0 gran 0 limit 0 flags c0000100 index 0
     PCI: 00:1f.1
     PCI: 00:1f.1 resource base c2b39000 size 100 align 12 gran 8 limit c2b390ff flags 60000201 index 10
     PCI: 00:1f.1 resource base 1040 size 20 align 5 gran 5 limit 105f flags 60000100 index 20
  Done allocating resources.
  BS: BS_DEV_RESOURCES times (us): entry 0 run 2959300 exit 0
  Enabling resources...
  PCI: 00:00.0 cmd <- 07
  PCI: 00:00.1 subsystem <- 8086/5a8c
  PCI: 00:00.1 cmd <- 02
  PCI: 00:00.2 subsystem <- 8086/5a8e
  PCI: 00:00.2 cmd <- 06
  PCI: 00:02.0 cmd <- 03
  PCI: 00:03.0 subsystem <- 8086/5a88
  PCI: 00:03.0 cmd <- 02
  PCI: 00:0d.1 cmd <- 07
  PCI: 00:0d.2 subsystem <- 8086/5a96
  PCI: 00:0d.2 cmd <- 406
  PCI: 00:0d.3 cmd <- 06
  PCI: 00:0e.0 cmd <- 02
  PCI: 00:0f.0 cmd <- 06
  PCI: 00:0f.1 cmd <- 06
  PCI: 00:0f.2 cmd <- 06
  PCI: 00:14.0 bridge ctrl <- 0003
  PCI: 00:14.0 cmd <- 06
  PCI: 00:15.0 cmd <- 02
  PCI: 00:16.0 cmd <- 02
  PCI: 00:16.1 cmd <- 02
  PCI: 00:16.2 cmd <- 06
  PCI: 00:16.3 cmd <- 02
  PCI: 00:17.0 cmd <- 02
  PCI: 00:17.1 cmd <- 02
  PCI: 00:18.0 cmd <- 02
  PCI: 00:18.1 cmd <- 02
  PCI: 00:18.2 cmd <- 06
  PCI: 00:19.0 subsystem <- 8086/5ac2
  PCI: 00:19.0 cmd <- 02
  PCI: 00:1b.0 cmd <- 06
  PCI: 00:1c.0 subsystem <- 8086/5acc
  PCI: 00:1c.0 cmd <- 06
  PCI: 00:1f.0 cmd <- 07
  PCI: 00:1f.1 subsystem <- 8086/5ad4
  PCI: 00:1f.1 cmd <- 03
  PCI: 01:00.0 subsystem <- 8086/095a
  PCI: 01:00.0 cmd <- 02
  done.
  BS: BS_DEV_ENABLE times (us): entry 162 run 112902 exit 0
  FMAP: area FPF_STATUS found @ 42f000 (4096 bytes)
  Initializing devices...
  Root Device init ...
  Root Device init finished in 2140 usecs
  CPU_CLUSTER: 0 init ...
  MTRR: Physical address space:
  0x0000000000000000 - 0x00000000000a0000 size 0x000a0000 type 6
  0x00000000000a0000 - 0x00000000000c0000 size 0x00020000 type 0
  0x00000000000c0000 - 0x000000007b800000 size 0x7b740000 type 6
  0x000000007b800000 - 0x00000000b0000000 size 0x34800000 type 0
  0x00000000b0000000 - 0x00000000c0000000 size 0x10000000 type 1
  0x00000000c0000000 - 0x0000000100000000 size 0x40000000 type 0
  0x0000000100000000 - 0x0000000180000000 size 0x80000000 type 6
  MTRR: Fixed MSR 0x250 0x0606060606060606
  MTRR: Fixed MSR 0x258 0x0606060606060606
  MTRR: Fixed MSR 0x259 0x0000000000000000
  MTRR: Fixed MSR 0x268 0x0606060606060606
  MTRR: Fixed MSR 0x269 0x0606060606060606
  MTRR: Fixed MSR 0x26a 0x0606060606060606
  MTRR: Fixed MSR 0x26b 0x0606060606060606
  MTRR: Fixed MSR 0x26c 0x0606060606060606
  MTRR: Fixed MSR 0x26d 0x0606060606060606
  MTRR: Fixed MSR 0x26e 0x0606060606060606
  MTRR: Fixed MSR 0x26f 0x0606060606060606
  call enable_fixed_mtrr()
  CPU physical address size: 39 bits
  MTRR: default type WB/UC MTRR counts: 6/8.
  MTRR: WB selected as default type.
  MTRR: 0 base 0x000000007b800000 mask 0x0000007fff800000 type 0
  MTRR: 1 base 0x000000007c000000 mask 0x0000007ffc000000 type 0
  MTRR: 2 base 0x0000000080000000 mask 0x0000007fe0000000 type 0
  MTRR: 3 base 0x00000000a0000000 mask 0x0000007ff0000000 type 0
  MTRR: 4 base 0x00000000b0000000 mask 0x0000007ff0000000 type 1
  MTRR: 5 base 0x00000000c0000000 mask 0x0000007fc0000000 type 0

  MTRR check
  Fixed MTRRs   : Enabled
  Variable MTRRs: Enabled

  Detected 4 core, 4 thread CPU.
  Will perform SMM setup.
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'cpu_microcode_blob.bin'
  CBFS: Found @ offset 11f00 size 4000
  microcode: sig=0x506c9 pf=0x1 revision=0x2c
  CPU: Intel(R) Celeron(R) CPU N3450 @ 1.10GHz.
  Loading module at 00030000 with entry 00030000. filesize: 0x130 memsize: 0x130
  Processing 16 relocs. Offset value of 0x00030000
  Attempting to start 3 APs
  Waiting for 10ms after sending INIT.
  Waiting for 1st SIPI to complete...done.
  Waiting for 2nd SIPI to complete...done.
  AP: slot 1 apic_id 6.
  AP: slot 3 apic_id 2.
  AP: slot 2 apic_id 4.
  Loading module at 00038000 with entry 00038000. filesize: 0x1a8 memsize: 0x1a8
  Processing 12 relocs. Offset value of 0x00038000
  SMM Module: stub loaded at 00038000. Will call 7ab90823(00000000)
  Installing SMM handler to 0x7b000000
  Loading module at 7b010000 with entry 7b010cfc. filesize: 0x3550 memsize: 0x7630
  Processing 229 relocs. Offset value of 0x7b010000
  Loading module at 7b008000 with entry 7b008000. filesize: 0x1a8 memsize: 0x1a8
  Processing 12 relocs. Offset value of 0x7b008000
  SMM Module: placing jmp sequence at 7b007c00 rel16 0x03fd
  SMM Module: placing jmp sequence at 7b007800 rel16 0x07fd
  SMM Module: placing jmp sequence at 7b007400 rel16 0x0bfd
  SMM Module: stub loaded at 7b008000. Will call 7b010cfc(00000000)
  Clearing SMI status registers
  New SMBASE 0x7afff400
  Relocation complete.
  New SMBASE 0x7afff800
  Relocation complete.
  New SMBASE 0x7b000000
  Relocation complete.
  New SMBASE 0x7afffc00
  Relocation complete.
  Initializing CPU #0
  CPU: vendor Intel device 506c9
  CPU: family 06, model 5c, stepping 09
  CPU #0 initialized
  Initializing CPU #3
  Initializing CPU #1
  CPU: vendor Intel device 506c9
  CPU: family 06, model 5c, stepping 09
  Initializing CPU #2
  CPU #3 initialized
  CPU: vendor Intel device 506c9
  CPU: family 06, model 5c, stepping 09
  CPU: vendor Intel device 506c9
  CPU: family 06, model 5c, stepping 09
  CPU #1 initialized
  CPU #2 initialized
  Enabling SMIs.
  MTRR: TEMPORARY Physical address space:
  0x0000000000000000 - 0x00000000000a0000 size 0x000a0000 type 6
  0x00000000000a0000 - 0x00000000000c0000 size 0x00020000 type 0
  0x00000000000c0000 - 0x000000007b800000 size 0x7b740000 type 6
  0x000000007b800000 - 0x00000000ff000000 size 0x83800000 type 0
  0x00000000ff000000 - 0x0000000100000000 size 0x01000000 type 5
  0x0000000100000000 - 0x0000000180000000 size 0x80000000 type 6
  MTRR: default type WB/UC MTRR counts: 10/8.
  MTRR: UC selected as default type.
  MTRR: 0 base 0x0000000000000000 mask 0x0000007fc0000000 type 6
  MTRR: 1 base 0x0000000040000000 mask 0x0000007fe0000000 type 6
  MTRR: 2 base 0x0000000060000000 mask 0x0000007ff0000000 type 6
  MTRR: 3 base 0x0000000070000000 mask 0x0000007ff8000000 type 6
  MTRR: 4 base 0x0000000078000000 mask 0x0000007ffc000000 type 6
  MTRR: 5 base 0x000000007b800000 mask 0x0000007fff800000 type 0
  MTRR: 6 base 0x00000000ff000000 mask 0x0000007fff000000 type 5
  MTRR: 7 base 0x0000000100000000 mask 0x0000007f00000000 type 6
  CPU_CLUSTER: 0 init finished in 460906 usecs
  PCI: 00:00.0 init ...
  PCI: 00:00.0 init finished in 2240 usecs
  PCI: 00:00.1 init ...
  PCI: 00:00.1 init finished in 2240 usecs
  PCI: 00:00.2 init ...
  PCI: 00:00.2 init finished in 2241 usecs
  PCI: 00:02.0 init ...
  PCI: 00:02.0 init finished in 2239 usecs
  PCI: 00:03.0 init ...
  PCI: 00:03.0 init finished in 2237 usecs
  PCI: 00:0d.1 init ...
  gpe0_sts[0]: 00000000 gpe0_en[0]: 00000000
  gpe0_sts[1]: 0fad13f7 gpe0_en[1]: 00000000
  gpe0_sts[2]: 3ffd0024 gpe0_en[2]: 00000000
  gpe0_sts[3]: 800ec00e gpe0_en[3]: 00000000
  Disabling ACPI via APMC:Done.
  SLP S3 assertion width: 50000 usecs
  PCI: 00:0d.1 init finished in 27404 usecs
  PCI: 00:0d.2 init ...
  PCI: 00:0d.2 init finished in 2239 usecs
  PCI: 00:0f.0 init ...
  PCI: 00:0f.0 init finished in 2242 usecs
  PCI: 00:0f.1 init ...
  PCI: 00:0f.1 init finished in 2238 usecs
  PCI: 00:0f.2 init ...
  PCI: 00:0f.2 init finished in 2239 usecs
  PCI: 00:15.0 init ...
  PCI: 00:15.0 init finished in 2238 usecs
  PCI: 00:16.0 init ...
  LPSS I2C bus 0 at 0xc2b21000 (400 KHz)
  PCI: 00:16.0 init finished in 6137 usecs
  PCI: 00:16.1 init ...
  LPSS I2C bus 1 at 0xc2b23000 (400 KHz)
  PCI: 00:16.1 init finished in 6130 usecs
  PCI: 00:16.2 init ...
  LPSS I2C bus 2 at 0xc2b25000 (400 KHz)
  PCI: 00:16.2 init finished in 6124 usecs
  PCI: 00:16.3 init ...
  LPSS I2C bus 3 at 0xc2b27000 (400 KHz)
  PCI: 00:16.3 init finished in 6132 usecs
  PCI: 00:17.0 init ...
  LPSS I2C bus 4 at 0xc2b29000 (400 KHz)
  PCI: 00:17.0 init finished in 6130 usecs
  PCI: 00:17.1 init ...
  LPSS I2C bus 5 at 0xc2b2b000 (400 KHz)
  PCI: 00:17.1 init finished in 6128 usecs
  PCI: 00:19.0 init ...
  PCI: 00:19.0 init finished in 2238 usecs
  PCI: 00:1c.0 init ...
  PCI: 00:1c.0 init finished in 2238 usecs
  PCI: 00:1f.0 init ...
  RTC Init
  PCI: 00:1f.0 init finished in 3561 usecs
  PCI: 00:1f.1 init ...
  PCI: 00:1f.1 init finished in 2240 usecs
  PCI: 01:00.0 init ...
  PCI: 01:00.0 init finished in 2355 usecs
  PNP: 0c09.0 init ...
  Keyboard init...
  PS/2 keyboard initialized on primary channel
  Google Chrome EC: Initializing keyboard.
  Google Chrome EC: Hello got back 11223344 status (0)
  Google Chrome EC: version:
      ro: coral_v1.1.7287-0dafc64dd
      rw: coral_v1.1.7302-d2b56e247
    running image: 1
  PNP: 0c09.0 init finished in 42254 usecs
  Devices initialized
  Show all devs... After init.
  Root Device: enabled 1
  CPU_CLUSTER: 0: enabled 1
  APIC: 00: enabled 1
  DOMAIN: 0000: enabled 1
  PCI: 00:00.0: enabled 1
  PCI: 00:00.1: enabled 1
  PCI: 00:00.2: enabled 1
  PCI: 00:02.0: enabled 1
  PCI: 00:03.0: enabled 1
  PCI: 00:0d.0: enabled 1
  PCI: 00:0d.1: enabled 1
  PCI: 00:0d.2: enabled 1
  PCI: 00:0d.3: enabled 1
  PCI: 00:0e.0: enabled 1
  GENERIC: 0.0: enabled 1
  PCI: 00:11.0: enabled 0
  PCI: 00:12.0: enabled 0
  PCI: 00:13.0: enabled 0
  PCI: 00:13.1: enabled 0
  PCI: 00:13.2: enabled 0
  PCI: 00:13.3: enabled 0
  PCI: 00:14.0: enabled 1
  PCI: 01:00.0: enabled 1
  PCI: 00:14.1: enabled 0
  PCI: 00:15.0: enabled 1
  PCI: 00:15.1: enabled 0
  PCI: 00:16.0: enabled 1
  I2C: 01:1a: enabled 1
  PCI: 00:16.1: enabled 1
  PCI: 00:16.2: enabled 1
  I2C: 02:50: enabled 1
  PCI: 00:16.3: enabled 1
  I2C: 03:10: enabled 1
  I2C: 03:39: enabled 1
  PCI: 00:17.0: enabled 1
  I2C: 04:15: enabled 1
  I2C: 04:2c: enabled 1
  PCI: 00:17.1: enabled 1
  I2C: 05:09: enabled 1
  PCI: 00:17.2: enabled 0
  PCI: 00:17.3: enabled 0
  PCI: 00:18.0: enabled 1
  PCI: 00:18.1: enabled 1
  PCI: 00:18.2: enabled 1
  PCI: 00:18.3: enabled 0
  PCI: 00:19.0: enabled 1
  PCI: 00:19.1: enabled 0
  PCI: 00:19.2: enabled 0
  PCI: 00:1a.0: enabled 0
  PCI: 00:1b.0: enabled 1
  PCI: 00:1c.0: enabled 1
  PCI: 00:1e.0: enabled 0
  PCI: 00:1f.0: enabled 1
  PNP: 0c09.0: enabled 1
  PCI: 00:1f.1: enabled 1
  PCI: 00:0f.0: enabled 1
  PCI: 00:0f.1: enabled 1
  PCI: 00:0f.2: enabled 1
  APIC: 06: enabled 1
  APIC: 04: enabled 1
  APIC: 02: enabled 1
  BS: BS_DEV_INIT times (us): entry 15122 run 875144 exit 893
  ELOG: Event(A0) added with size 9 at 2020-11-19 20:36:01 UTC
  elog_add_boot_reason: Logged dev mode boot
  Finalize devices...
  Devices finalized
  FMAP: area RW_NVRAM found @ 42a000 (20480 bytes)
  BS: BS_POST_DEVICE times (us): entry 10779 run 3965 exit 5254
  BS: BS_OS_RESUME_CHECK times (us): entry 0 run 60 exit 0
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fallback/dsdt.aml'
  CBFS: Found @ offset b7600 size 2e40
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fallback/slic'
  CBFS: 'fallback/slic' not found.
  ACPI: Writing ACPI tables at 7ab10000.
  ACPI:    * FACS
  ACPI:    * DSDT
  Ramoops buffer: 0x100000@0x7aa10000.
  ACPI:    * FADT
  SCI is IRQ9
  ACPI: added table 1/32, length now 40
  ACPI:     * SSDT
  Found 1 CPU(s) with 4 core(s) each.
  Turbo is available and visible
  PSS: 1101MHz power 6000 control 0x1600 status 0x1600
  PSS: 1100MHz power 6000 control 0xb00 status 0xb00
  PSS: 1000MHz power 5388 control 0xa00 status 0xa00
  PSS: 800MHz power 4213 control 0x800 status 0x800
  PSS: 1101MHz power 6000 control 0x1600 status 0x1600
  PSS: 1100MHz power 6000 control 0xb00 status 0xb00
  PSS: 1000MHz power 5388 control 0xa00 status 0xa00
  PSS: 800MHz power 4213 control 0x800 status 0x800
  PSS: 1101MHz power 6000 control 0x1600 status 0x1600
  PSS: 1100MHz power 6000 control 0xb00 status 0xb00
  PSS: 1000MHz power 5388 control 0xa00 status 0xa00
  PSS: 800MHz power 4213 control 0x800 status 0x800
  PSS: 1101MHz power 6000 control 0x1600 status 0x1600
  PSS: 1100MHz power 6000 control 0xb00 status 0xb00
  PSS: 1000MHz power 5388 control 0xa00 status 0xa00
  PSS: 800MHz power 4213 control 0x800 status 0x800
  \_SB.PCI0.HDAS.MAXM: Maxim Integrated 98357A Amplifier
  Error: Could not locate 'wifi_sar' in VPD
  Error: failed from getting SAR limits!
  \_SB.PCI0.RP01.WIFI: Intel WiFi PCI: 01:00.0
  lpss_i2c: bad counts. hcnt = -8 lcnt = 14
  \_SB.PCI0.I2C0.DLG7: Dialog Semiconductor DA7219 Audio Codec address 01ah irq 91
  lpss_i2c: bad counts. hcnt = -5 lcnt = 17
  \_SB.PCI0.I2C2.TPMI: I2C TPM at I2C: 02:50
  lpss_i2c: bad counts. hcnt = -1 lcnt = 77
  lpss_i2c: bad counts. hcnt = -23 lcnt = 32
  \_SB.PCI0.I2C3.D010: ELAN Touchscreen at I2C: 03:10
  \_SB.PCI0.I2C3.D039: Raydium Touchscreen at I2C: 03:39
  lpss_i2c: bad counts. hcnt = -1 lcnt = 72
  lpss_i2c: bad counts. hcnt = -23 lcnt = 27
  \_SB.PCI0.I2C4.D015: ELAN Touchpad at I2C: 04:15
  \_SB.PCI0.I2C4.H02C: Synaptics Touchpad at I2C: 04:2c
  lpss_i2c: bad counts. hcnt = -5 lcnt = 4
  \_SB.PCI0.I2C5.H009: WCOM Digitizer at I2C: 05:09
  ACPI: added table 2/32, length now 44
  ACPI:    * MCFG
  ACPI: added table 3/32, length now 48
  ACPI:    * TCPA
  TCPA log created at 7aa00000
  ACPI: added table 4/32, length now 52
  ACPI:    * MADT
  SCI is IRQ9
  ACPI: added table 5/32, length now 56
  current = 7ab14920
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'dmic-4ch-48khz-16b.bin'
  CBFS: Found @ offset 2e4c0 size be8
  Added 4CH DMIC array.
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'dialog-2ch-48khz-24b.bin'
  CBFS: Found @ offset 2f200 size 64
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'dialog-2ch-48khz-24b.bin'
  CBFS: Found @ offset 2f200 size 64
  Added Dialog_7219 codec.
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'max98357-render-2ch-48khz-24b.bin'
  CBFS: Found @ offset 2f100 size 74
  Added Maxim_98357 codec.
  ACPI:    * NHLT
  ACPI: added table 6/32, length now 60
  ACPI:    * IGD OpRegion
  ACPI:    * HPET
  ACPI: added table 7/32, length now 64
  ACPI: done.
  ACPI tables: 30656 bytes.
  smbios_write_tables: 7a9fd000
  Create SMBIOS type 17
  Root Device (Google Coral)
  CPU_CLUSTER: 0 (Intel Apollolake SOC)
  APIC: 00 (Intel Apollolake SOC)
  DOMAIN: 0000 (Intel Apollolake SOC)
  PCI: 00:00.0 (Intel Apollolake SOC)
  PCI: 00:00.1 (Intel Apollolake SOC)
  PCI: 00:00.2 (Intel Apollolake SOC)
  PCI: 00:02.0 (Intel Apollolake SOC)
  PCI: 00:03.0 (Intel Apollolake SOC)
  PCI: 00:0d.0 (Intel Apollolake SOC)
  PCI: 00:0d.1 (Intel Apollolake SOC)
  PCI: 00:0d.2 (Intel Apollolake SOC)
  PCI: 00:0d.3 (Intel Apollolake SOC)
  PCI: 00:0e.0 (Intel Apollolake SOC)
  GENERIC: 0.0 (Maxim Integrated 98357A Amplifier)
  PCI: 00:11.0 (Intel Apollolake SOC)
  PCI: 00:12.0 (Intel Apollolake SOC)
  PCI: 00:13.0 (Intel Apollolake SOC)
  PCI: 00:13.1 (Intel Apollolake SOC)
  PCI: 00:13.2 (Intel Apollolake SOC)
  PCI: 00:13.3 (Intel Apollolake SOC)
  PCI: 00:14.0 (Intel Apollolake SOC)
  PCI: 01:00.0 (Intel WiFi)
  PCI: 00:14.1 (Intel Apollolake SOC)
  PCI: 00:15.0 (Intel Apollolake SOC)
  PCI: 00:15.1 (Intel Apollolake SOC)
  PCI: 00:16.0 (Intel Apollolake SOC)
  I2C: 01:1a (Dialog Semiconductor DA7219 Audio Codec)
  PCI: 00:16.1 (Intel Apollolake SOC)
  PCI: 00:16.2 (Intel Apollolake SOC)
  I2C: 02:50 (I2C TPM)
  PCI: 00:16.3 (Intel Apollolake SOC)
  I2C: 03:10 (I2C Device)
  I2C: 03:39 (I2C Device)
  PCI: 00:17.0 (Intel Apollolake SOC)
  I2C: 04:15 (I2C Device)
  I2C: 04:2c (I2C HID Device)
  PCI: 00:17.1 (Intel Apollolake SOC)
  I2C: 05:09 (I2C HID Device)
  PCI: 00:17.2 (Intel Apollolake SOC)
  PCI: 00:17.3 (Intel Apollolake SOC)
  PCI: 00:18.0 (Intel Apollolake SOC)
  PCI: 00:18.1 (Intel Apollolake SOC)
  PCI: 00:18.2 (Intel Apollolake SOC)
  PCI: 00:18.3 (Intel Apollolake SOC)
  PCI: 00:19.0 (Intel Apollolake SOC)
  PCI: 00:19.1 (Intel Apollolake SOC)
  PCI: 00:19.2 (Intel Apollolake SOC)
  PCI: 00:1a.0 (Intel Apollolake SOC)
  PCI: 00:1b.0 (Intel Apollolake SOC)
  PCI: 00:1c.0 (Intel Apollolake SOC)
  PCI: 00:1e.0 (Intel Apollolake SOC)
  PCI: 00:1f.0 (Intel Apollolake SOC)
  PNP: 0c09.0 (Google Chrome EC)
  PCI: 00:1f.1 (Intel Apollolake SOC)
  PCI: 00:0f.0 (unknown)
  PCI: 00:0f.1 (unknown)
  PCI: 00:0f.2 (unknown)
  APIC: 06 (unknown)
  APIC: 04 (unknown)
  APIC: 02 (unknown)
  SMBIOS tables: 834 bytes.
  Writing table forward entry at 0x00000500
  Wrote coreboot table at: 00000500, 0x10 bytes, checksum 452b
  Writing coreboot table at 0x7ab34000
   0. 0000000000000000-0000000000000fff: CONFIGURATION TABLES
   1. 0000000000001000-000000000009ffff: RAM
   2. 00000000000a0000-00000000000fffff: RESERVED
   3. 0000000000100000-000000000fffffff: RAM
   4. 0000000010000000-0000000012150fff: RESERVED
   5. 0000000012151000-000000007a9fcfff: RAM
   6. 000000007a9fd000-000000007affffff: CONFIGURATION TABLES
   7. 000000007b000000-000000007fffffff: RESERVED
   8. 00000000d0000000-00000000d0ffffff: RESERVED
   9. 00000000e0000000-00000000efffffff: RESERVED
  10. 00000000fed10000-00000000fed17fff: RESERVED
  11. 0000000100000000-000000017fffffff: RAM
  Graphics framebuffer located at 0xb0000000
  Passing 6 GPIOs to payload:
              NAME |       PORT | POLARITY |     VALUE
     write protect |  undefined |     high |       low
          recovery |  undefined |     high |      high
               lid |  undefined |     high |       low
             power |  undefined |     high |      high
             oprom |  undefined |     high |      high
          EC in RW | 0x00000029 |     high |      high
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  Wrote coreboot table at: 7ab34000, 0x5cc bytes, checksum aed1
  coreboot table: 1508 bytes.
  IMD ROOT    0. 7afff000 00001000
  IMD SMALL   1. 7affe000 00001000
  FSP MEMORY  2. 7abfe000 00400000
  CONSOLE     3. 7abde000 00020000
  TIME STAMP  4. 7abdd000 00000400
  VBOOT       5. 7abdc000 00000c0c
  MRC DATA    6. 7abd0000 0000b2d0
  ROMSTG STCK 7. 7abcf000 00000400
  AFTER CAR   8. 7abc5000 0000a000
  RAMSTAGE    9. 7ab77000 0004e000
  REFCODE    10. 7ab4d000 0002a000
  ACPI GNVS  11. 7ab4c000 00001000
  SMM BACKUP 12. 7ab3c000 00010000
  COREBOOT   13. 7ab34000 00008000
  ACPI       14. 7ab10000 00024000
  RAMOOPS    15. 7aa10000 00100000
  TCPA LOG   16. 7aa00000 00010000
  EXT VBT17. 7a9fe000 0000180a
  SMBIOS     18. 7a9fd000 00000800
  IMD small region:
    IMD ROOT    0. 7affec00 00000400
    FSP RUNTIME 1. 7affebe0 00000004
    VBOOT SEL   2. 7affebc0 00000008
    EC HOSTEVENT 3. 7affeba0 00000004
    POWER STATE 4. 7affeb60 00000040
    ROMSTAGE    5. 7affeb40 00000004
    VARMRC DATA 6. 7affeb00 00000028
    MEM INFO    7. 7affe9a0 00000141
    GNVS PTR    8. 7affe980 00000004
  BS: BS_WRITE_TABLES times (us): entry 0 run 755779 exit 0
  Locality already claimed
  cr50 TPM 2.0 (i2c 2:0x50 id 0x28)
  Checking cr50 for pending updates
  TPM Command: 0x20000000
  7abb95c0: 80 01 00 00 00 0e 20 00 00 00 00 18 03 e8        ...... .......
  TPM Response: 0x00000000
  7abb95c0: 80 01 00 00 00 0d 00 00 00 00 00 18 00           .............
  CBFS: 'VBOOT' located CBFS at [440000:561ec0)
  CBFS: Locating 'fallback/payload'
  CBFS: Found @ offset d2a80 size 4f3ca
  Loading segment from ROM address 0xff593ab8
    code (compression=1)
    New segment dstaddr 0x1110000 memsize 0xba86b srcaddr 0xff593af0 filesize 0x4f392
  Loading segment from ROM address 0xff593ad4
    Entry Point 0x01110000
  Loading Segment: addr: 0x0000000001110000 memsz: 0x00000000000ba86b filesz: 0x000000000004f392
  lb: [0x000000007ab78000, 0x000000007abc4bb0)
  Post relocation: addr: 0x0000000001110000 memsz: 0x00000000000ba86b filesz: 0x000000000004f392
  using LZMA
  [ 0x01110000, 011ca86b, 0x011ca86b) <- ff593af0
  dest 01110000, end 011ca86b, bouncebuffer ffffffff
  Loaded segments
  CSE FWSTS1: 0x80000245
  CSE FWSTS2: 0x30850000
  CSE FWSTS3: 0x00000000
  CSE FWSTS4: 0x00080000
  CSE FWSTS5: 0x00000000
  CSE FWSTS6: 0x40000000
  ME: Manufacturing Mode      : NO
  ME: FPF status              : fused
  BS: BS_PAYLOAD_LOAD times (us): entry 36614 run 170372 exit 35023
  Jumping to boot code at 01110000(7ab34000)
  CPU0: stack: 7abb4000 - 7abb5000, lowest used address 7abb486c, stack used: 1940 bytes


  U-Boot 2021.04-rc4-00367-g94829ee2924-dirty (Apr 04 2021 - 09:18:41 +1200)

  CPU: x86_64, vendor Intel, device 506c9h
  DRAM:  3.9 GiB
  MMC:   emmc@1c,0: 0, pci_mmc: 1
  Video: 1024x768x32
  Vendor: Google
  Model: Coral
  BIOS Version:
  BIOS date: 11/18/2020
  Net:   No ethernet found.
  Bus xhci_pci: Register f000820 NbrPorts 15
  Starting the controller
  USB XHCI 1.00
  scanning bus xhci_pci for devices... 3 USB Device(s) found
  Finalizing coreboot
  Hit any key to stop autoboot:  0
  Running stage 'rw_init'

  Starting vboot on Coral...
  Booting from slot A: vboot->ctx=79765990, flags 8
  Cr50 does not have an ready GPIO/interrupt (err=-19)
  SF: Detected w25q128fw with page size 256 Bytes, erase size 4 KiB, total 16 MiB
  FMAP at 204000, length 1000
  Found shared_data_blob at 7abdc00c, size 3072
  Running stage 'rw_selectkernel'
  tpm_get_response: command 0x14e, return code 0x0
  RollbackKernelRead: TPM: RollbackKernelRead 10001
  tpm_get_response: command 0x14e, return code 0x28b
  RollbackFwmpRead: TPM: no FWMP space
  print_hash: RW(active) hash: 197d05ba0ed650b7001f31938ee53302fd9721a8397d09e7e645fc2d47472c7e
  sync_one_ec: devidx=0 select_rw=4
  sync_one_ec: jumping to EC-RW
  vb2_developer_ui: Entering
  No panel found (cannot adjust backlight)
  cbgfx initialised: screen:width=1024, height=768, offset=0 canvas:width=768, height=768, offset=128
  Supported locales:  en, es-419, pt-BR, fr, es, pt-PT, ca, it, de, el, nl, da, nb, sv, fi, et, lv, lt, ru, pl, cs, sk, hu, sl, sr, hr, bg, ro, uk, tr, he, ar, fa, hi, th, ms, vi, id, fil, zh-CN, zh-TW, ko, ja, bn, gu, kn, ml, mr, ta, te, (50 locales)
  Load locale file 'vbgfx.bin'
  Load locale file 'font.bin'
  Load locale file 'locale_en.bin'
  vb2_audio_start: vb2_audio_start() - using short dev screen delay
  vb2_developer_ui: VbBootDeveloper() - trying fixed disk
  VbTryLoadKernel: VbTryLoadKernel() start, get_info_flags=0x2
  MMC: no card present
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
  tpm_get_response: command 0x121, return code 0x0
  VbSelectAndLoadKernel: Returning 0
  Running stage 'rw_bootkernel'
  partition_number=2, guid=35c775e7-3735-d745-93e5-d9e0238f7ed0
  Bloblist:
  Address       Size  Tag Name
  Updating ACPI tables
  Updating SMBIOS table at 7a9fd000
  Kernel command line: "cros_secure root=/dev/sdb3 init=/sbin/init rootwait ro console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 root=PARTUUID=35c775e7-3735-d745-93e5-d9e0238f7ed0/PARTNROFF=1 rootwait rw dm_verity.error_behavior=3 dm_verity.max_bios=-1 dm_verity.dev_wait=0 dm="1 vroot none rw 1,0 3788800 verity payload=ROOT_DEV hashtree=HASH_DEV hashstart=3788800 alg=sha1 root_hexdigest=55052b629d3ac889f25a9583ea12cdcd3ea15ff8 salt=a2d4d9e574069f4fed5e3961b99054b7a4905414b60a25d89974a7334021165c" noinitrd vt.global_cursor_default=0 kern_guid=35c775e7-3735-d745-93e5-d9e0238f7ed0 add_efi_memmap boot=local noresume noswap i915.modeset=1 tpm_tis.force=1 tpm_tis.interrupts=0 nmi_watchdog=panic,lapic disablevmx=off  "

  Starting kernel ...
