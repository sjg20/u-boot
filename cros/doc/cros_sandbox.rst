.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2020 Google LLC

Vboot on sandbox
================

This provides a few details about running vboot on sandbox, useful for
development and testing. See :doc:`../../arch/sandbox` for more information on
sandbox.

This uses version of vboot from late 2018. Since then vboot has had a code
clean-up to simplify the handoff information in 2019 and a new 'dark' UI in
2020.


Build and run
-------------

To obtain::

   git clone https://github.com/sjg20/u-boot.git
   cd u-boot
   git checkout cros-2021.01

   cd ..
   git clone https://chromium.googlesource.com/chromiumos/platform/vboot_reference
   cd vboot_reference
   git checkout 45964294
   #  futility: updater: Correct output version for Snow

To build for sandbox::

   UB=/tmp/b/chromeos_sandbox    # U-Boot build directory
   cd u-boot
   make O=$UB chromeos_sandbox_defconfig
   make O=$UB -j20 -s VBOOT_SOURCE=/path/to/vboot_reference \
     MAKEFLAGS_VBOOT=DEBUG=1 QUIET=1

To run on sandbox::

   CROS=~/cosarm

   # Use an ARM build so it ends up at a 'bootm' command, since sandbox does not
   # support the x86 'zboot' command.
   IMG=$CROS/src/build/images/v/latest/chromiumos_image.bin
   $UB/tpl/u-boot-tpl -d $UB/u-boot.dtb.out \
     -L6 -c "host bind 0 $IMG; vboot go auto" \
     -l -w -s state.dtb -r -n -m $UB/ram

   $UB/tpl/u-boot-tpl -d $UB/u-boot.dtb.out -L6 -l \
     -c "host bind 0 $IMG; vboot go auto" -w -s $UB/state.dtb -r -n -m $UB/mem


Boot flow
---------

This implementation fits into U-Boot's existing flow, which proceeds in order:

   - TPL - Tertiary program loader, a very small loader which is responsible
     just for loading SPL. This typically runs in SRAM.
   - SPL - Secondary program loader, typically runs in SRAM. It sets up SDRAM
     and loads U-Boot proper. In 'Falcon mode' it can boot a kernel directly
     with limited features
   - U-Boot - full bootloader, responsible for loading the kernel and booting

Sandbox uses the following phases:

   - TPL - this runs most of the vboot flow, dealing with firmware selection
     through to starting the selected SPL
   - SPL - simply boots U-Boot
   - U-Boot - runs the vboot UI if needed and pretends to boot the kernel


Emulators
---------

Sandbox uses emulators for a many devices and about 70 emulators are included
in U-Boor. For vboot, these are of particular interest:

   - TPM - sandbox provides TPM1 and TPM2 implementations. Vboot uses TPM1 at
     present. See drivers/tpm/tpm_tis_sandbox.c
   - EC - sandbox provides an emulation of the Chromium OS EC. It includes a
     reasonable set of features but only the subset needed to boot. See
     drivers/cros_ec/cros_ec_sandbox.c
   - SPI flash - emulates various SPI-flash parts, as identified by U-Boot's
     identification code. See drivers/mtd/spi/sandbox.c


System state
------------

All emulators are available read and write state (the state.dtb file in the
command line above). For example, the EC can keep nvdata there and the TPM can
keep secdata there. The SANDBOX_STATE_IO() macro declares handlers for reading
and writing state. This allows for repeatable tests, or for multiple phases to
pass information to each other.

Sandbox's emulated memory can be read on startup and written on exit, for a
similar purpose. Data structures that are placed in memory, such as the
bloblist, can thereby be passed along to the next phase.


Devicetree
----------

All devices and emulators are defined in one place: the devicetree. The binman
flashmap is also there, along with any vboot settings that are not hard-coded
in Kconfig. It is generally possible to configure the system there. Of course,
the build system can customise the devicetree independently from the U-Boot
build.


Sample run
----------

This shows the output from a sample sandbox run, which ends up faking a kernel
boot, with quite a bit of debugging included::

  U-Boot TPL 2021.04-rc1-00132-gc16f723a9cc-dirty (Feb 12 2021 - 22:07:30 -0700)
  cros_do_stage() start
  vboot_run_stage() Running stage 'ver_init'
  Get flags index 0x108
  Vboot nvdata:
     Signature v1, size 16 (valid), CRC 20 (calc 20, valid)
     - kernel settings reset
     - firmware settings reset
     Result 0, prev 0
     Recovery 0, subcode 0
     Localization 0, default boot 0, kernel 0, max roll-forward 0
  tpm: nvread index=0x1007, len=0xa, seq=0x1
  tpm_secdata_read() TPM has no secdata for index: returning err=-2
  Get flags index 0x108
  tpm_clear_and_reenable() TPM: Clear and re-enable
  cros_nvdata_write_walk() write type 2 size d
  tpm: nvwrite index=0x1008, len=0xd
  cros_nvdata_write_walk() write type 1 size a
  tpm: nvwrite index=0x1007, len=0xa
  Vboot secdata:
  00000000: 02 00 00 00 00 00 00 00 00 f2                      ..........
     Size 10 : valid
     CRC f2 (calc f2): valid
     Version 2
     Firmware versions 0
  00000000: 02 00 00 00 00 00 00 00 00 f2                      ..........
  vboot_ver_init() Enabled developer mode
  flag_gpio_probe() Sandbox gpio lid-open/2 = 1
  vboot_flag_read_walk_prev() No flag device for recovery
  vboot_flag_read_walk_prev() No flag device for wipeout
  save_if_needed() Saving secdata
  cros_nvdata_write_walk() write type 1 size a
  tpm: nvwrite index=0x1007, len=0xa
  vboot_run_stage() Running stage 'ver1_vbinit'
  resource_read() GBB: Reading SPI flash offset=29ee3f, size=80
  vb2_check_recovery: Recovery reason from previous boot: 0x0 / 0x0
  vb2ex_tpm_clear_owner() Clearing TPM owner
  tpm_clear_and_reenable() TPM: Clear and re-enable
  save_if_needed() Saving secdata
  cros_nvdata_write_walk() write type 1 size a
  tpm: nvwrite index=0x1007, len=0xa
  vboot_run_stage() Running stage 'ver2_selectfw'
  vboot_run_stage() Running stage 'ver3_tryfw'
  resource_read() GBB: Reading SPI flash offset=29efbf, size=1000
  vb2_report_dev_firmware: This is developer signed firmware
  resource_read() Slot A: Reading SPI flash offset=3dffee, size=70
  resource_read() Slot A: Reading SPI flash offset=3dffee, size=8b8
  vb2_verify_keyblock: Checking key block signature...
  resource_read() Slot A: Reading SPI flash offset=3e08a6, size=6c
  resource_read() Slot A: Reading SPI flash offset=3e08a6, size=874
  vb2_verify_fw_preamble: Verifying preamble.
  vboot_run_stage() Running stage 'ver4_locatefw'
  vboot_ver4_locate_fw() Setting up firmware reader at 3e1fee, size 308eb0
  hash_body() Hashing firmware body, expected size 308eb0
  vb2api_init_hash: HW crypto for hash_alg 2 not supported, using SW
  handle_digest_result() is_resume=0
  cros_nvdata_write_walk() write type 4 size 40
  cros_nvdata_write_walk() Failed to write type 4
  vboot_save_hash() write: returning err=-38
  handle_digest_result() Error -38 saving vboot hash
  vboot_run_stage() Running stage 'ver5_finishfw'
  tpm: nvwrite index=0x0, len=0x0
  vboot_ver5_finish_fw() Slot A is selected
  vboot_fill_handoff() Creating vboot_handoff structure
  fill_handoff() Copying FW preamble
  vboot_fill_handoff() flags a8 recovery=0, EC=cros-ec
  vboot_run_stage() Running stage 'ver_jump'
  vboot_jump() Reading firmware offset 3e1fee, length 308eb0
  state_uninit() Writing sandbox state
  sandbox: starting...
  debug: main
  size=30, ptr=30, limit=8000: 7fb1af4ab000

  U-Boot SPL 2021.04-rc1-00132-gc16f723a9cc-dirty (Feb 12 2021 - 22:07:30 -0700)
  cros_do_stage() start
  vboot_run_stage() Running stage 'spl_init'
  vboot_run_stage() Running stage 'spl_jump_u_boot'
  vboot_jump() Reading firmware offset 6eae9e, length 669016
  state_uninit() Writing sandbox state
  sandbox: starting...
  debug: main
  size=30, ptr=30, limit=8000: 7fc8a7ac4000
  pinctrl_select_state_full() sandbox_serial serial: pinctrl_select_state_full: uclass_get_device_by_phandle_id: err=-19


  U-Boot 2021.04-rc1-00132-gc16f723a9cc-dirty (Feb 12 2021 - 22:07:30 -0700)

  Model: sandbox
  DRAM:  128 MiB

  Warning: host_lo MAC addresses don't match:
  Address in ROM is		c6:b2:31:b8:5a:40
  Address in environment is	00:00:11:22:33:44

  Warning: host_docker0 MAC addresses don't match:
  Address in ROM is		62:51:2d:18:67:f0
  Address in environment is	00:00:11:22:33:45
  pinctrl_select_state_full() sandbox_serial serial: pinctrl_select_state_full: pinctrl_config_one: err=-38
  MMC:
  In:    cros-ec-keyb
  Out:   vidconsole
  Err:   vidconsole
  Model: sandbox
  binman_select_subnode() binman: Selected image subnode 'read-only'
  SCSI:
  Net:   eth0: host_lo, eth1: host_enp5s0, eth2: host_eth6, eth3: host_docker0, eth4: host_docker_gwbridge, eth5: eth@10002000
  vboot_run_stage() Running stage 'rw_init'
  vboot_rw_init() flags a8 0
  SF: Detected w25q128 with page size 256 Bytes, erase size 4 KiB, total 16 MiB
  common_params_init() Found shared_data_blob at 58a714c, size 3072
  flag_gpio_probe() Sandbox gpio lid-open/2 = 1
  vboot_run_stage() Running stage 'rw_selectkernel'
  TlclRead: TPM: TlclRead(0x1008, 13)
  tpm: nvread index=0x1008, len=0xd, seq=0x2
  TlclSendReceiveNoRetry: TPM: command 0xcf returned 0x0
  Get flags index 0x1008
  TlclSendReceiveNoRetry: TPM: command 0x65 returned 0x0
  RollbackKernelRead: TPM: RollbackKernelRead 0
  TlclRead: TPM: TlclRead(0x100a, 40)
  tpm: nvread index=0x100a, len=0x28, seq=0x4
  TlclSendReceiveNoRetry: TPM: command 0xcf returned 0x2
  RollbackFwmpRead: TPM: no FWMP space
     ** Unknown EC command 0xa0
  cros_ec_read_limit_power() PARAM_LIMIT_POWER not supported by EC
  vb2_developer_ui: Entering
  vboot_init_screen() No panel found (cannot adjust backlight)
  cbgfx_init() cbgfx initialised: screen:width=1366, height=768, offset=0 canvas:width=768, height=768, offset=299
  vboot_init_locale() Supported locales:  en, es-419, pt-BR, fr, es, pt-PT, ca, it, de, el, nl, da, nb, sv, fi, et, lv, lt, ru, pl, cs, sk, hu, sl, sr, hr, bg, ro, uk, tr, he, ar, fa, hi, th, ms, vi, id, fil, zh-CN, zh-TW, ko, ja, bn, gu, kn, ml, mr, ta, te, (50 locales)
  load_archive() Load locale file 'vbgfx.bin'
  load_archive() Load locale file 'font.bin'
  load_archive() Load locale file 'locale_en.bin'
  vb2_developer_ui: VbBootDeveloper() - user pressed Ctrl+D; skip delay
  vb2_developer_ui: VbBootDeveloper() - trying fixed disk
  VbTryLoadKernel: VbTryLoadKernel() start, get_info_flags=0x2
  VbExDiskGetInfo() Found 1 disks
  VbTryLoadKernel: VbTryLoadKernel() found 1 disks
  VbTryLoadKernel: VbTryLoadKernel() trying disk 0
  GptNextKernelEntry: GptNextKernelEntry looking at new prio partition 2
  GptNextKernelEntry: GptNextKernelEntry s1 t15 p15
  GptNextKernelEntry: GptNextKernelEntry looking at new prio partition 4
  GptNextKernelEntry: GptNextKernelEntry s0 t0 p0
  GptNextKernelEntry: GptNextKernelEntry looking at new prio partition 6
  GptNextKernelEntry: GptNextKernelEntry s0 t0 p0
  GptNextKernelEntry: GptNextKernelEntry likes partition 2
  LoadKernel: Found kernel entry at 20480 size 32768
  vb2_verify_keyblock: Checking key block signature...
  vb2_verify_digest: Wrong data signature size for algorithm, sig_size=1024, expected 512 for algorithm 4.
  vb2_verify_keyblock: Invalid key block signature.
  vb2_verify_kernel_vblock: Verifying key block signature failed.
  vb2_verify_keyblock_hash: Checking key block hash...
  vb2_verify_kernel_vblock: Key block recovery flag mismatch.
  vb2_verify_kernel_preamble: Verifying kernel preamble.
  vb2_verify_kernel_vblock: Kernel preamble is good.
  vb2_load_partition: Partition is good.
  LoadKernel: Key block valid: 0
  LoadKernel: Combined version: 65537
  LoadKernel: In recovery mode or dev-signed kernel
  LoadKernel: Good partition 2
  VbTryLoadKernel: VbTryLoadKernel() LoadKernel() = 0
  TlclLockPhysicalPresence: TPM: Lock physical presence
  TlclSendReceiveNoRetry: TPM: command 0x4000000a returned 0x0
  VbSelectAndLoadKernel: Returning 0
  vboot_run_stage() Running stage 'rw_bootkernel'
  boot_kernel() partition_number=2, guid=f1a84645-e9ea-7142-91ce-6fcdcf971422
  boot_kernel() Bloblist:
  Address       Size  Tag Name
  058a7030        20    2 SPL hand-off
  058a7060        d0    3 Chrome OS vboot context
  058a7140       c0c    4 Chrome OS vboot hand-off
  ## Loading kernel from FIT Image at 01008000 ...
     Using 'conf@1' configuration
     Verifying Hash Integrity ... OK
     Trying 'kernel@1' kernel subimage
       Description:  unavailable
       Created:      2019-07-14   7:20:19 UTC
       Type:         Kernel Image (no loading done)
       Compression:  uncompressed
       Data Start:   0x010080c8
       Data Size:    4533504 Bytes = 4.3 MiB
     Verifying Hash Integrity ... OK
  ## Loading fdt from FIT Image at 01008000 ...
     Using 'conf@1' configuration
     Verifying Hash Integrity ... OK
     Trying 'fdt@1' fdt subimage
       Description:  rk3066a-bqcurie2.dtb
       Created:      2019-07-14   7:20:19 UTC
       Type:         Flat Device Tree
       Compression:  uncompressed
       Data Start:   0x0145ae7c
       Data Size:    13397 Bytes = 13.1 KiB
       Architecture: ARM
       Hash algo:    sha1
       Hash value:   5379c8bd2c019f85151c1145ee177fbab6922f39
     Verifying Hash Integrity ... sha1+ OK
     Booting using the fdt blob at 0x145ae7c
     XIP Kernel Image (no loading done)
  ## Transferring control to Linux (at address 010080c8)...
  sandbox: continuing, as we cannot run Linux
  vboot_run_stage() Error: stage 'rw_bootkernel' returned 1
  vboot_run_stages() Cold reboot
