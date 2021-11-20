# SPDX-License-Identifier: GPL-2.0+
# Copyright 2019 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for the ARM Trusted Firmware Firmware Image Package (FIP)
# format

from collections import OrderedDict

from binman.entry import Entry
from binman.fip_util import fip_types, FipWriter
from dtoc import fdt_util

class Entry_atf_fip(Entry):
    """ARM Trusted Firmware Firmware Image Package (FIP)
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self.align_default = None
        self._fip_align = fdt_util.GetInt(node, 'fip-align', 1)
        self._fip_entries = OrderedDict()
        self._ReadSubnodes()

    def _ReadSubnodes(self):
        """Read the subnodes to find out what should go in this CBFS"""
        for node in self._node.subnodes:
            fip_type = None
            if node.name in fip_types:
                fip_type = node.name
                etype = 'blob-ext'
            entry = Entry.Create(self, node, etype)
            entry._fip_type = fip_type
            entry.ReadNode()
            entry._fip_name = node.name
            self._fip_entries[entry._fip_name] = entry

    def ObtainContents(self, skip=None):
        fip = FipWriter(self._fip_align)
        for entry in self._fip_entries.values():
            # First get the input data and put it in a file. If not available,
            # try later.
            if entry != skip and not entry.ObtainContents():
                return False
            data = entry.GetData()
            fipf = fip.add_file(entry._fip_type, entry.data, 0)
            if fipf:
                entry._fip_file = fipf
        data = fip.get_data()
        self.SetContents(data)
        return True
