# SPDX-License-Identifier:      GPL-2.0+
# Copyright 2024 Google LLC
# Written by Simon Glass <sjg@chromium.org>

"""Entry-type module for producing multiple alternate sections"""

import glob
import os

from binman.entry import EntryArg
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
        self._cur_fdt = None
        self._fdt_phase = None
        self.fdtgrep = None

    def ReadNode(self):
        """Read properties from the node"""
        super().ReadNode()
        self._fdt_dir = fdt_util.GetString(self._node, 'fdt-list-dir')
        fname = tools.get_input_filename(self._fdt_dir)
        fdts = glob.glob('*.dtb', root_dir=fname)
        self._fdts = [os.path.splitext(f)[0] for f in fdts]

        self._fdt_phase = fdt_util.GetString(self._node, 'fdt-phase')
        self.alternates = self._fdts

        self._fname_pattern = fdt_util.GetString(self._node, 'filename-pattern')

        self._remove_props = []
        props, = self.GetEntryArgsOrProps(
            [EntryArg('of-spl-remove-props', str)], required=False)
        if props:
            self._remove_props = props.split()

    def FdtContents(self, fdt_etype):
        if not self._cur_fdt:
            return self.section.FdtContents(fdt_etype)

        fname = os.path.join(self._fdt_dir, f'{self._cur_fdt}.dtb')
        infile = tools.get_input_filename(fname)
        if self._fdt_phase:
            uniq = self.GetUniqueName()
            outfile = tools.get_output_filename(
                f'{uniq}.{self._cur_fdt}-{self._fdt_phase}.dtb')
            self.fdtgrep.create_for_phase(infile, self._fdt_phase, outfile,
                                          self._remove_props)
            return outfile, tools.read_file(outfile)
        return fname, tools.read_file(infile)

    def ProcessWithFdt(self, alt):
        pattern = self._fname_pattern or 'NAME.bin'
        fname = pattern.replace('NAME', alt)

        data = b''
        try:
            self._cur_fdt = alt
            self.ProcessContents()
            data = self.GetPaddedData()
        finally:
            self._cur_fdt = None
        return fname, data

    def AddBintools(self, btools):
        super().AddBintools(btools)
        self.fdtgrep = self.AddBintool(btools, 'fdtgrep')

