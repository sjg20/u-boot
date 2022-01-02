# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for mkimage"""

from binman import bintool
from patman import tools

class Bintoolmkimage(bintool.Bintool):
    """Handles the 'mkimage' tool"""
    def __init__(self, name):
        super().__init__(name)
        self.toolname = 'mkimage'

    def run(self, reset_timestamp=False, output_fname=None, external=False,
            pad=None):
        """Run mkimage

        Args:
            reset_timestamp: True to update the timestamp in the FIT
            output_fname: Output filename to write to
            external: True to create an 'external' FIT, where the binaries are
                located outside the main data structure
            pad: Bytes to use for padding the FIT devicetree output. This allows
                other things to be easily added later, if required, such as
                signatures
        """
        args = []
        if external:
            args.append('-E')
        if pad:
            args += ['-p', f'{pad:x}']
        if reset_timestamp:
            args.append('-t')
        if output_fname:
            args += ['-F', output_fname]
        tools.Run(self.toolname, *args)
