# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for expanded U-Boot TPL binary
#

from binman.etype.blob_phase import Entry_blob_phase

class Entry_u_boot_tpl_expanded(Entry_blob_phase):
    """U-Boot TPL flat binary broken out into its component parts

    This is a section containing the U-Boot binary, BSS padding if needed and a
    devicetree.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, 'section', node, 'u-boot-tpl',
                         'u-boot-tpl-dtb', True)
