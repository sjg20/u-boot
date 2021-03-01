# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for U-Boot binary
#

from binman.etype.blob_phase import Entry_blob_phase

class Entry_u_boot_expanded(Entry_blob_phase):
    """U-Boot flat binary

    Properties / Entry arguments:
        - filename: Filename of u-boot.bin (default 'u-boot.bin')

    This is the U-Boot binary, containing relocation information to allow it
    to relocate itself at runtime. The binary typically includes a device tree
    blob at the end of it.

    U-Boot can access binman symbols at runtime. See:

        'Access to binman entry offsets at run time (fdt)'

    in the binman README for more information.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, 'section', node, 'u-boot', 'u-boot-dtb',
                         False)
