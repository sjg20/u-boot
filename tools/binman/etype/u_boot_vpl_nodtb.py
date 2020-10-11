# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for 'u-boot-vpl-nodtb.bin'
#

from binman import elf
from binman.entry import Entry
from binman.etype.blob import Entry_blob
from dtoc import fdt_util
from patman import tools

class Entry_u_boot_vpl_nodtb(Entry_blob):
    """VPL binary without device tree appended

    Properties / Entry arguments:
        - filename: Filename of vpl/u-boot-vpl-nodtb.bin (default
            'vpl/u-boot-vpl-nodtb.bin')

    This is the U-Boot VPL binary, It does not include a device tree blob at
    the end of it so may not be able to work without it, assuming VPL needs
    a device tree to operation on your platform. You can add a u_boot_vpl_dtb
    entry after this one, or use a u_boot_vpl entry instead (which contains
    both VPL and the device tree).
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self.spl_pad = fdt_util.GetBool(self._node, 'spl-pad')

    def ObtainContents(self):
        if not super().ObtainContents():
            return False
        fname = tools.GetInputFilename('vpl/u-boot-vpl')
        bss_size = elf.GetSymbolAddress(fname, '__bss_size')
        if not bss_size:
            self.Raise('Expected __bss_size symbol in vpl/u-boot-vpl')
        self.SetContents(self.data + tools.GetBytes(0, bss_size))
        return True

    def GetDefaultFilename(self):
        return 'vpl/u-boot-vpl-nodtb.bin'
