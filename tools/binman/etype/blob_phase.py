# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type base class for U-Boot or SPL binary with devicetree
#

from binman.etype.section import Entry_section

class Entry_blob_phase(Entry_blob):
    def __init__(self, section, etype, node, root_fname, dtb_file, bss_pad):

