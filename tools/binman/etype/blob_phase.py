# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type base class for U-Boot or SPL binary with devicetree
#

from binman.etype.blob import Entry_blob

class Entry_blob_phase(Entry_blob):
    def __init__(self, section, etype, node, root_fname, dtb_file, bss_pad):
        """Set up a new blob for a phase

        This holds an executable for a U-Boot phase, optional BSS padding and
        a devicetree

        Args:
            section: entry_Section object for this entry's parent
            etype: Type of object
            node: Node defining this entry
            root_fname: Root filename for the binary ('u-boot',
                'spl/u-boot-spl', etc.)
            dtb_file: Name of devicetree file ('u-boot.dtb', u-boot-spl.dtb',
                etc.)
            bss_pad: True to add BSS padding before the devicetree
        """
        # Put this here to allow entry-docs and help to work without libfdt
        global state
        from binman import state

        super().__init__(section, etype, node)
        self.root_fname = root_fname
        self.dtb_file = dtb_file
        self.bss_pad = bss_pad

    def GetFdtEtype(self):
        return self.dtb_file

    def _BuildPhase(self):
        """Get the full file contents"""

        # Get the executable first
        self._filename = self.GetDefaultFilename()
        self._pathname, _ = state.GetFdtContents(self.GetFdtEtype())
        bin_data = tools.ReadFile(self._pathname)

        # Now add BSS padding if needed
        if self.bss_pad:
            bss_size = elf.GetSymbolAddress(root_fname, '__bss_size')
            if not bss_size:
                self.Raise('Expected __bss_size symbol in spl/u-boot-spl')
            pad_data = tools.GetBytes(0, bss_size)
        else:
            pad_data = b''

        # Now the devicetree
        _, dtb_data = state.GetFdtContents(self.GetFdtEtype())

        indata = bin_data + pad_data + dtb_data
        data = self.CompressData(indata)
        return data

    def ObtainContents(self):
        """Get the full file contents"""
        data = self._BuildPhase()
        self.SetContents(data)
        return True

    def ProcessContents(self):
        """Get the full file contents"""
        data = self._BuildPhase()
        return self.ProcessContentsUpdate(data)

    def GetFdts(self):
        """Get the device trees used by this entry

        Returns:
            Dict:
                key: Filename from this entry (without the path)
                value: Tuple:
                    Fdt object for this dtb, or None if not available
                    Filename of file containing this dtb
        """
        fname = self.basename + '.dtb'
        return {self.GetFdtEtype(): [self, fname]}

    def WriteData(self, data, decomp=True):
        ok = super().WriteData(data, decomp)

        # Update the state module, since it has the authoritative record of the
        # device trees used. If we don't do this, then state.GetFdtContents()
        # will still return the old contents
        state.UpdateFdtContents(self.GetFdtEtype(), data)
        return ok
