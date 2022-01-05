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

usage: fiptool [--verbose] <command> [<args>]
Global options supported:
  --verbose	Enable verbose output for all commands.

Commands supported:
  info		List images contained in FIP.
  create	Create a new FIP with the given images.
  update	Update an existing FIP with the given images.
  unpack	Unpack images from FIP.
  remove	Remove images from FIP.
  version	Show fiptool version.
  help		Show help for given command.

"""

from binman import bintool

class Bintoolfiptool(bintool.Bintool):
    """Handles the 'fiptool' tool"""
    def __init__(self, name):
        super().__init__(name, 'Manipulate ATF FIP files')

    def info(self, fname):
        """Get info on a FIP image

        Args:
            fname (str): Filename to check

        Returns:
            str: Tool output
        """
        args = ['info', fname]
        return self.run_cmd(*args)

    def create_new(self, fname, align, plat_toc_flags, fwu, tb_fw, blob_uuid,
                   blob_file):
        args = [
            'create',
            '--align', f'{align:x}',
            '--plat-toc-flags', f'{plat_toc_flags:#x}',
            '--fwu', fwu,
            '--tb-fw', tb_fw,
            '--blob', f'uuid={blob_uuid},file={blob_file}',
            fname]
        return self.run_cmd(*args)

    def create_bad(self):
        args = ['create', '--fred']
        return self.run_cmd(*args)

    def fetch(self, method):
        if method != bintool.FETCH_BUILD:
            return None
        fname, tmpdir = self.build_from_git(
            'https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git',
            'fiptool',
            'tools/fiptool/fiptool')
        return fname, tmpdir

    def version(self):
        out = self.run_cmd('version').strip()
        return out
