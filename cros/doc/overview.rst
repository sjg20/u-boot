.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2018 Google LLC

Introduction
============

Chromium OS uses a verified-boot system, called vboot, which checks that all
code that runs on the device has been signed by the owner and that as much code
as possible can be updated in the field.

U-Boot is a widely used and highly functional bootloader which runs on a large
variety of modern platforms.

The `cros/` sub-directory includes an implementation of Chromium OS verified
boot for U-Boot. It makes use of the Open-Source vboot_reference code in the
Chromium OS tree. It includes various drivers and logic to allow a device to
boot using vboot.
