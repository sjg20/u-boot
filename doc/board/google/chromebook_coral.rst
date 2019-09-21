.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Simon Glass <sjg@chromium.org>

Chromebook Coral
================

Here are some random notes, to be expanded.

Hob size returned from FSP is about 53KB.

Partial ROM map

fef07000	TPL/SPL Stack top
fef10000
fef16000 2a000	FSP M default stack
fef40000	SPL
fef71000 59000	FSP M
fefca000

Partial memory map

CONFIG_BLOBLIST_ADDR=0x100000


[1] Intel PDF https://www.coreboot.org/images/2/23/Apollolake_SoC.pdf
