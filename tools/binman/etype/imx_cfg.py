# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
# Entry-type module for generating the iMX8 .cfg file
#

from collections import OrderedDict

from binman.entry import Entry
from binman.etype.section import Entry_section
from binman.etype.blob import Entry_blob
from dtoc import fdt_util
from patman import tools

class Entry_imx_cfg(Entry_section):
    """iMX .cfg file

    Properties / Entry arguments:
        - boot_from - device to boot from (e.g. 'sd')
        -

    This entry is valid for PowerPC mpc85xx cpus. This entry holds
    'bootpg + resetvec' code for PowerPC mpc85xx CPUs which needs to be
    placed at offset 'RESET_VECTOR_ADDRESS - 0xffc'.
    """

    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self._entries = OrderedDict()
        self.required_props = ['boot-from', 'boot-addr']

    def ReadNode(self):
        super().ReadNode()
        self.boot_from = fdt_util.GetString(self._node, 'boot-from')
        self.boot_addr = fdt_util.GetInt(self._node, 'boot-addr')
        self.rom_version = fdt_util.GetInt(self._node, 'rom-version')
        self.ReadEntries()

    def ReadEntries(self):
        """Read the subnodes to find out what should go in this image"""
        for node in self._node.subnodes:
            entry = Entry.Create(self, node)
            entry.ReadNode()
            self._entries[entry.name] = entry

    def _build_input(self):
        #def ObtainContents(self, fake_size=0):
        _, input_fname, uniq = self.collect_contents_to_file(
            self._entries.values(), 'input')
        output_fname = tools.get_output_filename('imx.cfg.%s' % uniq)
        with open(output_fname, 'w') as outf:
            print('ROM_VERSION v%x' % self.rom_version, file=outf)
            print('BOOT_FROM %s' % self.boot_from, file=outf)
            print('LOADER %s %#x' % (input_fname, self.boot_addr), file=outf)
        data = tools.read_file(output_fname)
        self.SetContents(data)
        return True

    def BuildSectionData(self, required):
        self._build_input()
        return self.data

    '''
    def SetAllowMissing(self, allow_missing):
        """Set whether a section allows missing external blobs

        Args:
            allow_missing: True if allowed, False if not allowed
        """
        self.allow_missing = allow_missing
        for entry in self._entries.values():
            entry.SetAllowMissing(allow_missing)

    def SetAllowFakeBlob(self, allow_fake):
        """Set whether the sub nodes allows to create a fake blob

        Args:
            allow_fake: True if allowed, False if not allowed
        """
        super().SetAllowFakeBlob(allow_fake)
        for entry in self._entries.values():
            entry.SetAllowFakeBlob(allow_fake)

    def CheckFakedBlobs(self, faked_blobs_list):
        """Check if any entries in this section have faked external blobs

        If there are faked blobs, the entries are added to the list

        Args:
            faked_blobs_list: List of Entry objects to be added to
        """
        for entry in self._entries.values():
            entry.CheckFakedBlobs(faked_blobs_list)

    def BuildSectionData(self, required):
        return self.data
    '''
