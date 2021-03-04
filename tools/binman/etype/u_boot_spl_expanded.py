# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for expanded U-Boot SPL binary
#

from binman.etype.blob_phase import Entry_blob_phase

class Entry_u_boot_spl_expanded(Entry_blob_phase):
    """U-Boot SPL flat binary broken out into its component parts

    This is a section containing the U-Boot binary, BSS padding if needed and a
    devicetree.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, 'section', node, 'u-boot-spl',
                         'u-boot-spl-dtb', True)
