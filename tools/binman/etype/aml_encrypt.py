# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for producing an image using aml-encrypt-g12a
#

from collections import OrderedDict

from binman.entry import Entry
from binman.etype.section import Entry_section
from binman.etype.blob_ext import Entry_blob_ext
from binman.etype.blob_ext_list import Entry_blob_ext_list
from dtoc import fdt_util
from patman import tools
from patman import tout

DDR_FW_COUNT = 9

class Entry_aml_encrypt(Entry_section):
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self._entries = OrderedDict()
        self.align_default = None
        self._aml_algo = None
        self._aml_op = None
        self._aml_level = None

    def ReadNode(self):
        super().ReadNode()
        self._aml_algo = fdt_util.GetString(self._node, 'aml-algo')
        self._aml_op = fdt_util.GetString(self._node, 'aml-op')
        self._aml_level = fdt_util.GetString(self._node, 'aml-level')
        self._aml_input = fdt_util.GetString(self._node, 'aml-input')
        self._aml_compress = fdt_util.GetString(self._node, 'aml-compress')
        self._aml_type = fdt_util.GetString(self._node, 'aml-type')
        #self._aml_ddrfw = {}
        #for i in range(1, DDR_FW_COUNT + 1):
            #self._aml_ddrfw[i] = fdt_util.GetString(self._node, f'aml-ddrfw{i}')
        self.ReadEntries()

    def ReadEntries(self):
        """Read the subnodes to find out what should go in this image"""
        for node in self._node.subnodes:
            etype = None
            if node.name.startswith('aml-') and 'type' not in node.props:
                etype = 'blob-ext'
            entry = Entry.Create(self, node, etype)
            entry.ReadNode()
            self._entries[entry.name] = entry

    def BuildSectionData(self, required):
        uniq = self.GetUniqueName()
        output_fname = tools.GetOutputFilename('aml-out.%s' % uniq)
        args = [f'aml_encrypt_{self._aml_algo}',
            f'--{self._aml_op}',
            '--output', output_fname
            ]
        if self._aml_level:
            args += ['--level', f'{self._aml_level}']
        if self._aml_compress:
            args += ['--compress', f'{self._aml_compress}']
        if self._aml_type:
            args += ['--type', f'{self._aml_type}']
        if self._aml_input:
            input_pathname = tools.GetInputFilename(
                self._aml_input,
                self.external and self.section.GetAllowMissing())
            if not input_pathname:
                missing = True
            args += ['--input', f'{input_pathname}']

        missing = False
        for entry in self._entries.values():
            # First get the input data and put it in a file. If not available,
            # try later.
            entry_data = entry.GetData(required)
            if not required and entry_data is None:
                return None
            flag_name = entry.name.replace('aml-', '')  # Drop the aml- prefix
            if isinstance(entry, Entry_blob_ext_list):
                for i, pathname in enumerate(entry._pathnames):
                    args += [f'--{flag_name}{i + 1}', pathname]
            elif isinstance(entry, Entry_blob_ext):
                pathname = entry._pathname
                args += [f'--{flag_name}', pathname]
            else:
                data = self.GetPaddedDataForEntry(entry, entry_data)
                fname = tools.GetOutputFilename('aml-in.%s' %
                                                entry.GetUniqueName())
                tools.WriteFile(fname, data)
                args += [f'--{flag_name}', fname]
            if entry.missing:
                missing = True

        if missing:
            self.missing = True
            return b''

        tout.Debug(f"Node '{self._node.path}': running: %s" % ' '.join(args))
        tools.Run(*args)

        # If an input file (or subnode!) is providing the input, the tools
        # writes to the requested output file. Otherwise it uses the output file
        # as a template for three files that it writes, ending in '.sd.bin',
        # 'usb.bl2' and 'usb.tpl'. We use the first one as the image output
        if self._aml_input or self._node.FindNode('aml-input'):
            real_outfile = output_fname
        else:
            real_outfile = f'{output_fname}.sd.bin'
        data = tools.ReadFile(real_outfile)
        return data

    def SetAllowMissing(self, allow_missing):
        self.allow_missing = allow_missing

    def SetImagePos(self, image_pos):
        Entry.SetImagePos(self, image_pos)

    def SetCalculatedProperties(self):
        Entry.SetCalculatedProperties(self)

    def CheckEntries(self):
        Entry.CheckEntries(self)
