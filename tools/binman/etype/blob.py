# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for blobs, which are binary objects read from files
#

from collections import OrderedDict

from binman.entry import Entry
from binman import state
from dtoc import fdt_util
from patman import tools
from patman import tout

class Entry_blob(Entry):
    """Arbitrary binary blob

    Note: This should not be used by itself. It is normally used as a parent
    class by other entry types.

    Properties / Entry arguments:
        - filename: Filename of file to read into entry
        - compress: Compression algorithm to use:
            none: No compression
            lz4: Use lz4 compression (via 'lz4' command-line utility)

    This entry reads data from a file and places it in the entry. The
    default filename is often specified specified by the subclass. See for
    example the 'u-boot' entry which provides the filename 'u-boot.bin'.

    If compression is enabled, an extra 'uncomp-size' property is written to
    the node (if enabled with -u) which provides the uncompressed size of the
    data.
    """
    def __init__(self, section, etype, node, auto_write_symbols=False):
        super().__init__(section, etype, node,
                         auto_write_symbols=auto_write_symbols)
        self._filename = fdt_util.GetString(self._node, 'filename', self.etype)
        self._entries = OrderedDict()

    def ReadNode(self):
        super().ReadNode()
        self.ReadEntries()

    def ReadEntries(self):
        for node in self._node.subnodes:
            if node.name.startswith('hash') or node.name.startswith('signature'):
                continue
            entry = Entry.Create(self.section, node,
                                 expanded=self.GetImage().use_expanded,
                                 missing_etype=self.GetImage().missing_etype)
            entry.ReadNode()
            self._entries[node.name] = entry

    def _PackEntries(self):
        """Pack all entries into the blob"""
        offset = 0
        for entry in self._entries.values():
            offset = entry.Pack(offset)
        return offset

    def Pack(self, offset):
        """Pack all entries into the blob"""
        self._PackEntries()

        offset = super().Pack(offset)
        self.CheckEntries()
        return offset

    def ObtainContents(self, fake_size=0):
        self._filename = self.GetDefaultFilename()
        self._pathname = tools.get_input_filename(self._filename,
            self.external and (self.optional or self.section.GetAllowMissing()))
        # Allow the file to be missing
        if not self._pathname:
            self._pathname, faked = self.check_fake_fname(self._filename,
                                                          fake_size)
            self.missing = True
            if not faked:
                self.SetContents(b'')
                return True

        self.ReadBlobContents()
        return True

    def BuildBlobData(self, required):
        # Override with any local entries
        data = self.data
        for entry in self._entries.values():
            if entry.ObtainContents() is False:
                return False
            entry_data = entry.GetData(True)
            if entry_data is not None:
                data = (data[:entry.offset] + entry_data +
                        data[entry.offset + len(entry_data):])

        return data

    def GetData(self, required=True):
        data = self.BuildBlobData(required)
        if data is False or data is None:
            return None
        self.SetContents(data)
        return data

    def ReadFileContents(self, pathname):
        """Read blob contents into memory

        This function compresses the data before returning if needed.

        We assume the data is small enough to fit into memory. If this
        is used for large filesystem image that might not be true.
        In that case, Image.BuildImage() could be adjusted to use a
        new Entry method which can read in chunks. Then we could copy
        the data in chunks and avoid reading it all at once. For now
        this seems like an unnecessary complication.

        Args:
            pathname (str): Pathname to read from

        Returns:
            bytes: Data read
        """
        state.TimingStart('read')
        indata = tools.read_file(pathname)
        state.TimingAccum('read')
        state.TimingStart('compress')
        data = self.CompressData(indata)
        state.TimingAccum('compress')
        return data

    def ReadBlobContents(self):
        data = self.ReadFileContents(self._pathname)
        self.SetContents(data)
        return True

    def GetDefaultFilename(self):
        return self._filename

    def ProcessContents(self):
        # The blob may have changed due to WriteSymbols()
        return self.ProcessContentsUpdate(self.data)

    def CheckFakedBlobs(self, faked_blobs_list):
        """Check if any entries in this section have faked external blobs

        If there are faked blobs, the entries are added to the list

        Args:
            fake_blobs_list: List of Entry objects to be added to
        """
        if self.faked:
            faked_blobs_list.append(self)
