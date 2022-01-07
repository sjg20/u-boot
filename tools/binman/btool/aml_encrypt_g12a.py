# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for aml_encrypt_g12a

aml_encrypt_g12a provides a way to package firmware for Amlogic devices.

Documentation is not really available so far as I can tell

Source code also does not seem to be available, but there are some alternate
open-source tools in C:

https://github.com/afaerber/meson-tools
    GXBB, GXL & GXM only
https://github.com/repk/gxlimg
    GXBB, GXL, GXM & AXG only
https://github.com/angerman/meson64-tools
    developed for G12B, should work on G12A & SM1

Here is the help:

    AMLOGIC-G12A-G12B-SIG-module : Ver-0.07 Built on Nov  1 2018 17:12:32

    /tmp/aml_encrypt_g12a --rsagen --keylen len --exp exp --ned rkey --nex puk --nxd prk --txt tkey --aes akey
            keylen        : RSA key length:1024,2048,4096
            exp           : Exponent of RSA:3,0x10001,0x1374B
            ned           : RSA key file name
            nex           : RSA PUK file name
            nxd           : RSA PRVK file name
            txt           : RSA key file with text format
            aes           : AES key file without IV,GX series used only
            output        : signatured keymax

    /tmp/aml_encrypt_g12a --keysig --rkey1 rsakey [--rkey2 ..][--rkey3 ..][--rkey4 ..] --ukey1 rsakey --skey rsakey --output sig-rsa-key
            rkey1[2,3,4]  : root RSA public key for create keymax
            ukey1         : user RSA public key for create keymax
            skey          : root RSA private key for signature keymax
            output        : signatured keymax

    /tmp/aml_encrypt_g12a --keybnd --ukey file --rootkeymax file [--aeskey file] --output file
            ukey       : user RSA key to be used for secure boot
            rootkeymax : signatured keymax for secure boot
            aeskey     : AES key to be used for secure boot
            output     : keys package for secure boot

    /tmp/aml_encrypt_g12a --bootsig --input file --amluserkey file --output file
            input       : uboot image to be processed for normal or secure boot
            amluserkey  : amlogic key package for secure boot
            output      : signatured and encrypted uboot image

    /tmp/aml_encrypt_g12a --imgsig --input file --amluserkey file --output file
            input      : image to be processed for secure boot
            amluserkey : amlogic key package for secure boot
            output     : signatured and encrypted kernel image

    /tmp/aml_encrypt_g12a --binsig --input file --amluserkey file --output file
            input	   : binary file to be processed for secure boot
            amluserkey : amlogic key package for secure boot
            output	   : signatured and encrypted binary file

Note that the aml_encrypt_g12a and aml_encrypt_g12b executables appear to be
identical. Perhaps the executable name affects the behaviour?
"""

import re

from binman import bintool

class Bintoolaml_encrypt_g12a(bintool.Bintool):
    """Handles the 'aml_encrypt_g12a' tool"""
    def __init__(self, name):
        super().__init__(name, 'Amlogic encrypt g12a')

    def version(self):
        lines = self.run_cmd('', raise_on_error=False).strip().splitlines()
        if not lines:
            return super().version()
        out = lines[0]
        # AMLOGIC-G12A-G12B-SIG-module : Ver-0.07 Built on Nov  1 2018 17:12:32
        m_version = re.match(r'.*Ver-([^ ]*).*', out)
        return m_version.group(1).strip() if m_version else out

    def fetch(self, method):
        if method != bintool.FETCH_BIN:
            return None
        fname, tmpdir = self.fetch_from_drive(
            '1o_OiFB5Lf9ib-nPuQDLub8OPuJ0mKIum')
        return fname, tmpdir
