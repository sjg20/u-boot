# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for U-Boot binary
#

from binman.etype.blob_phase import Entry_blob_phase

class Entry_u_boot_expanded(Entry_blob_phase):
    """U-Boot flat binary broken out into its component parts

    This is a section containing the U-Boot binary and a devicetree.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, 'section', node, 'u-boot', 'u-boot-dtb',
                         False)
