# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for x86 VGA ROM binary blob
#

from binman.etype.blob_ext import Entry_blob_ext

class Entry_tegra_bct(Entry_blob_ext):
    """Entry containing an Nvidia Tegra Board Configuration Table (BCT)

    Properties / Entry arguments:
        - filename: Filename of file to read into entry

    This file contains low-level setup information include SDRAM parameters.
    It can be built from source, but for now binman does not support this.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
