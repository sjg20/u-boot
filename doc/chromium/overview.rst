Chromium OS Support in U-Boot
=============================

Introduction
------------

This describes how to use U-Boot with Chromium OS. Several options are
available:

   - Running U-Boot from the 'altfw' feature, which is available on selected
     Chromebooks from 2019 onwards (initially Grunt). Press '1' from the
     developer-mode screen to get into U-Boot. See here for details:
     https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_mode.md

   - Running U-Boot from the disk partition. This involves signing U-Boot and
     placing it on the disk, for booting as a 'kernel'. See
     :doc:`chainload` for information on this. This is the only
     option on non-U-Boot Chromebooks from 2013 to 2018 and is somewhat
     more involved.

   - Running U-Boot with Chromium OS verified boot. This allows U-Boot to be
     used instead of either or both of depthcharge (a bootloader which forked
     from U-Boot in 2013) and coreboot. See :doc:`run_vboot` for more
     information on this.

   - Running U-Boot from coreboot. This allows U-Boot to run on more devices
     since many of them only support coreboot as the bootloader and have
     no bare-metal support in U-Boot. For this, use the 'coreboot' target.

   - Running U-Boot and booting into a Chrome OS image, but without verified
     boot. This can be useful for testing.
