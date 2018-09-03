.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2018 Google LLC

Data files
==========

The data/ directory contains data files used by verified boot. In most cases
they are test files and not suitable for actual use.

devkeys
   developer keys taken from Chromium OS, used for self-signed images

locales
   locate information for verified boot (2018 vintage). The list of
   locales is in `locales`. The .bin files contain images and test for
   each locale. The `font.bin` file contains font images. The `vbgfx.bin`
   file contains generic images used by all locales. The file format is
   a coreboot archive, a very simple list of named files stored one
   after the other. See `cb_archive.h` for details

bmpblk.bin
   empty file simulating a bitmap block file

ecrw.bin
   empty file simulating a Chromium OS EC image

tianocore.bmp
   bitmap logo for Tianocore (for altfw)

tianocore.bmp
   PNG logo for Tianocore (for altfw)
