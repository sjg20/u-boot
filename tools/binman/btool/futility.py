# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for futility

futility (flash utility) is a tool for working with Chromium OS flash images.

Documentation is at: https://chromium.googlesource.com/chromiumos/platform/vboot/+/refs/heads/main/_vboot_reference/README

Source code: https://chromium.googlesource.com/chromiumos/platform/vboot/+/refs/heads/master/_vboot_reference/futility

Here is the help:
Usage: futility [options] COMMAND [args...]

This is the unified firmware utility, which will eventually replace
most of the distinct verified boot tools formerly produced by the
vboot_reference package.

When symlinked under the name of one of those previous tools, it should
fully implement the original behavior. It can also be invoked directly
as futility, followed by the original name as the first argument.

Global options:

  --vb1        Use only vboot v1.0 binary formats
  --vb21       Use only vboot v2.1 binary formats
  --debug      Be noisy about what's going on

The following commands are built-in:

  bdb                  Common boot flow utility
  create               Create a keypair from an RSA .pem file
  dump_fmap            Display FMAP contents from a firmware image
  dump_kernel_config   Prints the kernel command line
  gbb                  Manipulate the Google Binary Block (GBB)
  gbb_utility          Legacy name for `gbb` command
  help                 Show a bit of help (you're looking at it)
  load_fmap            Replace the contents of specified FMAP areas
  pcr                  Simulate a TPM PCR extension operation
  show                 Display the content of various binary components
  sign                 Sign / resign various binary components
  update               Update system firmware
  validate_rec_mrc     Validates content of Recovery MRC cache
  vbutil_firmware      Verified boot firmware utility
  vbutil_kernel        Creates, signs, and verifies the kernel partition
  vbutil_key           Wraps RSA keys with vboot headers
  vbutil_keyblock      Creates, signs, and verifies a keyblock
  verify               Verify the signatures of various binary components
  version              Show the futility source revision and build date
"""

from binman import bintool
from patman import tools

class Bintoolfutility(bintool.Bintool):
    """Handles the 'futility' tool"""
    def __init__(self, name):
        super().__init__(name)
        self.toolname = 'futility'
        self.desc = 'Chromium OS firmware utility'

    def run(self, version=False):
        args = []
        if version:
            args.append('version')
        return self.run_cmd(*args)

    def version(self):
        out = self.run(version=True).strip()
        return out
