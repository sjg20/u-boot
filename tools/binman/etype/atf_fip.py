# SPDX-License-Identifier: GPL-2.0+
# Copyright 2019 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for the ARM Trusted Firmware Firmware Image Package (FIP)
# format

from collections import OrderedDict

from binman.entry import Entry
from binman.fip_util import FIP_TYPES, FipReader, FipWriter, UUID_LEN
from dtoc import fdt_util

class Entry_atf_fip(Entry):
    """ARM Trusted Firmware Firmware Image Package (FIP)
    """
    def __init__(self, section, etype, node):
        # Put this here to allow entry-docs and help to work without libfdt
        global state
        from binman import state

        super().__init__(section, etype, node)
        self.align_default = None
        self._fip_flags = fdt_util.GetInt64(node, 'fip-hdr-flags', 0)
        self._fip_align = fdt_util.GetInt(node, 'fip-align', 1)
        self._fip_entries = OrderedDict()
        self._ReadSubnodes()
        self.reader = None

    def _ReadSubnodes(self):
        """Read the subnodes to find out what should go in this FIP"""
        for node in self._node.subnodes:
            fip_type = None
            etype = None
            if node.name in FIP_TYPES:
                fip_type = node.name
                etype = 'blob-ext'

            entry = Entry.Create(self, node, etype)
            entry.fip_uuid = fdt_util.GetBytes(node, 'fip-uuid', UUID_LEN)
            if not fip_type and not entry.fip_uuid:
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
            fent = fip.add_entry(entry._fip_type or entry.fip_uuid, entry.data,
                                 entry._fip_flags)
            if fent:
                entry._fip_entry = fent
        data = fip.get_data()
        self.SetContents(data)
        return True

    def SetImagePos(self, image_pos):
        """Override this function to set all the entry properties from FIP

        We can only do this once image_pos is known

        Args:
            image_pos: Position of this entry in the image
        """
        super().SetImagePos(image_pos)

        # Now update the entries with info from the FIP entries
        for entry in self._fip_entries.values():
            fent = entry._fip_entry
            entry.size = fent.size
            entry.offset = fent.offset
            entry.image_pos = self.image_pos + entry.offset

    def AddMissingProperties(self, have_image_pos):
        super().AddMissingProperties(have_image_pos)
        for entry in self._fip_entries.values():
            entry.AddMissingProperties(have_image_pos)

    def SetCalculatedProperties(self):
        """Set the value of device-tree properties calculated by binman"""
        super().SetCalculatedProperties()
        for entry in self._fip_entries.values():
            state.SetInt(entry._node, 'offset', entry.offset)
            state.SetInt(entry._node, 'size', entry.size)
            state.SetInt(entry._node, 'image-pos', entry.image_pos)

    def ListEntries(self, entries, indent):
        """Override this method to list all files in the section"""
        super().ListEntries(entries, indent)
        for entry in self._fip_entries.values():
            entry.ListEntries(entries, indent + 1)

    def GetEntries(self):
        return self._fip_entries

    def ReadData(self, decomp=True):
        data = super().ReadData(True)
        return data

    def ReadChildData(self, child, decomp=True):
        if not self.reader:
            self.fip_data = super().ReadData(True)
            self.reader = FipReader(self.fip_data)
        reader = self.reader
        #print('child', child, child.offset, child.size, len(self.fip_data))
        #fent = reader.fents.get(child.name)
        return self.fip_data[child.offset:child.offset + child.size]

    def WriteChildData(self, child):
        self.ObtainContents(skip=child)
        return True
