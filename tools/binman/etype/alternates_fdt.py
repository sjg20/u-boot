# SPDX-License-Identifier:      GPL-2.0+
# Copyright 2024 Google LLC
# Written by Simon Glass <sjg@chromium.org>

"""Entry-type module for producing multiple alternate sections"""

import glob
import os

from binman.etype.section import Entry_section
from dtoc import fdt_util
from u_boot_pylib import tools

class Entry_alternates_fdt(Entry_section):
    """Entry that generates alternative sections for each devicetree provided

    blah
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self.fdt_list_dir = None
        self.filename_pattern = None
        self.required_props = ['fdt-list-dir']

    def ReadNode(self):
        """Read properties from the node"""
        super().ReadNode()
        self._fdt_dir = fdt_util.GetString(self._node, 'fdt-list-dir')
        fname = tools.get_input_filename(self._fdt_dir)
        fdts = glob.glob('*.dtb', root_dir=fname)
        self._fdts = [os.path.splitext(f)[0] for f in fdts]

