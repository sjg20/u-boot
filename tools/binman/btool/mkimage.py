# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for mkimage"""

import re

from binman import bintool
from patman import tools

class Bintoolmkimage(bintool.Bintool):
    """Handles the 'mkimage' tool"""
    def __init__(self, name):
        super().__init__(name)
        self.toolname = 'mkimage'
        self.desc = 'Generate image for U-Boot'

    def run(self, reset_timestamp=False, output_fname=None, external=False,
            pad=None, version=False):
        """Run mkimage

        Args:
            reset_timestamp: True to update the timestamp in the FIT
            output_fname: Output filename to write to
            external: True to create an 'external' FIT, where the binaries are
                located outside the main data structure
            pad: Bytes to use for padding the FIT devicetree output. This allows
                other things to be easily added later, if required, such as
                signatures
            version: True to get the mkimage version
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
        if version:
            args.append('-V')
        return tools.Run(self.toolname, *args)

    def fetch(self):
        print("Build U-Boot to get tools/mkimage or sudo apt install u-boot-tools")
        return None

    def version(self):
        out = self.run(version=True).strip()
        m_version = re.match(r'mkimage version (.*)', out)
        return m_version.group(1) if m_version else out
