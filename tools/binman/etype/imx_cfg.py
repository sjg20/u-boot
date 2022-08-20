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
        self._section_size = None

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

    def BuildSectionData(self, required):
        #def ObtainContents(self, fake_size=0):
        data, input_fname, uniq = self.collect_contents_to_file(
            self._entries.values(), 'input')
        if data is None:
            if not required:
                return None
            return b''
        self._section_size = len(data)
        output_fname = tools.get_output_filename('imx.cfg.%s' % uniq)
        with open(output_fname, 'w') as outf:
            print('ROM_VERSION v%x' % self.rom_version, file=outf)
            print('BOOT_FROM %s' % self.boot_from, file=outf)
            print('LOADER %s %#x' % (input_fname, self.boot_addr), file=outf)
        data = tools.read_file(output_fname)
        return data

    def CheckEntries(self, section_size=None):
        """Override this to use a special section size

        The data produced by the nodes in this entry is not actually the data
        returned by this entry. In fact, the data is written to a file (see
        BuildSectionData()) and a small amount of data containing the filename
        of that file is what actually becomes the contents.

        The standard implenetation of CheckEntries() checks that the section
        size (typically under 100 bytes here) is smaller than the contents,
        which it is not. So pass the correct section size.
        """
        super().CheckEntries(section_size=self._section_size)
