# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for fiptool

fiptool provides a way to package firmware in an ARM Trusted Firmware Firmware
Image Package (ATF FIP) format. It is used with Trusted Firmware A, for example.

Documentation is at:
https://trustedfirmware-a.readthedocs.io/en/latest/getting_started/tools-build.html?highlight=fiptool#building-and-using-the-fip-tool

Source code is at:
https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git

Here is the help:
"""

from binman import bintool

class Bintoolfiptool(bintool.Bintool):
    """Handles the 'fiptool' tool"""
    def __init__(self, name):
        super().__init__(name, 'Manipulate ATF FIP files')

    def fetch(self, method):
        if method != bintool.FETCH_BUILD:
            return None
        fname, tmpdir = self.build_from_git(
            'https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git',
            'fiptool',
            'tools/fiptool/fiptool')
        return result, tmpdir

    def version(self):
        out = self.run_cmd('version').strip()
        return out
