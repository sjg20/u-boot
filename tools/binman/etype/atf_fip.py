# SPDX-License-Identifier: GPL-2.0+
# Copyright 2019 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for the ARM Trusted Firmware Firmware Image Package (FIP)
# format

from collections import OrderedDict

from binman.entry import Entry
from binman.fip_util import FIP_TYPES, FipWriter
from dtoc import fdt_util

class Entry_atf_fip(Entry):
    """ARM Trusted Firmware Firmware Image Package (FIP)
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self.align_default = None
        self._fip_flags = fdt_util.GetInt64(node, 'fip-hdr-flags', 0)
        self._fip_align = fdt_util.GetInt(node, 'fip-align', 1)
        self._fip_entries = OrderedDict()
        self._ReadSubnodes()

    def _ReadSubnodes(self):
        """Read the subnodes to find out what should go in this CBFS"""
        for node in self._node.subnodes:
            fip_type = None
            etype = None
            if node.name in FIP_TYPES:
                fip_type = node.name
                etype = 'blob-ext'

            entry = Entry.Create(self, node, etype)
            if not fip_type:
                fip_type = fdt_util.GetString(node, 'fip-type')
                if not fip_type:
                    self.Raise("Must provide a fip-type (node name '%s' is not a known FIP type)" %
                               node.name)

            entry._fip_type = fip_type
            entry._fip_flags = fdt_util.GetInt64(node, 'fip-flags', 0)
            entry.ReadNode()
            entry._fip_name = node.name
            self._fip_entries[entry._fip_name] = entry

    def ObtainContents(self, skip=None):
        fip = FipWriter(self._fip_flags, self._fip_align)
        for entry in self._fip_entries.values():
            # First get the input data and put it in a file. If not available,
            # try later.
            if entry != skip and not entry.ObtainContents():
                return False
            data = entry.GetData()
            fent = fip.add_entry(entry._fip_type, entry.data, entry._fip_flags)
            if fent:
                entry._fip_entry = fent
        data = fip.get_data()
        self.SetContents(data)
        return True
