# SPDX-License-Identifier: GPL-2.0+
# Copyright 2023 Marek Vasut <marex@denx.de>
#
# Entry-type module for generating the i.MX8M imx8mimage configuration file
#

from collections import OrderedDict

from binman.entry import Entry
from binman.etype.mkimage import Entry_mkimage
from binman import elf
from dtoc import fdt_util
from u_boot_pylib import tools

class Entry_nxp_imx8mimage_cfg(Entry_mkimage):
    """NXP i.MX8M imx8mimage .cfg file generator

    Properties / Entry arguments:
        - nxp,boot-from - device to boot from (e.g. 'sd')
        - nxp,rom-version - BootROM version ('2' for i.MX8M Nano and Plus)
    """

    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self.required_props = ['nxp,boot-from', 'nxp,rom-version', 'nxp,loader-address']

    def ReadNode(self):
        super().ReadNode()
        self.boot_from = fdt_util.GetString(self._node, 'nxp,boot-from')
        self.rom_version = fdt_util.GetInt(self._node, 'nxp,rom-version')
        self.loader_address = fdt_util.GetInt(self._node, 'nxp,loader-address')
        self.ReadEntries()

    def BuildSectionData(self, required):
        _, input_fname, uniq = self.collect_contents_to_file(
            self._entries.values(), 'input')
        print('build', input_fname, b'BSYM' in tools.read_file(input_fname))
        cfg_fname = tools.get_output_filename('nxp.imx8mimage.cfg.%s' % uniq)
        with open(cfg_fname, 'w') as outf:
            print('ROM_VERSION v%d' % self.rom_version, file=outf)
            print('BOOT_FROM %s' % self.boot_from, file=outf)
            print('LOADER %s %#x' % (input_fname, self.loader_address), file=outf)

        output_fname = tools.get_output_filename(f'cfg-out.{uniq}')
        args = ['-d', input_fname, '-n', cfg_fname, '-T', 'imx8mimage',
                output_fname]
        #print('args', args)
        if self.mkimage.run_cmd(*args) is not None:
            return tools.read_file(output_fname)
        else:
            # Bintool is missing; just use the input data as the output
            self.record_missing_bintool(self.mkimage)
            return data
