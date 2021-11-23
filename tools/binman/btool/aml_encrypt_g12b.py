# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for aml_encrypt_g12b

aml_encrypt_g12b provides a way to package firmware for Amlogic devices.

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

    aml_encrypt_g12b --rsagen --keylen len --exp exp --ned rkey --nex puk
        --nxd prk --txt tkey --aes akey
            keylen        : RSA key length:1024,2048,4096
            exp           : Exponent of RSA:3,0x10001,0x1374B
            ned           : RSA key file name
            nex           : RSA PUK file name
            nxd           : RSA PRVK file name
            txt           : RSA key file with text format
            aes           : AES key file without IV,GX series used only
            output        : signatured keymax

    aml_encrypt_g12b --keysig --rkey1 rsakey [--rkey2 ..][--rkey3 ..]
        [--rkey4 ..] --ukey1 rsakey --skey rsakey --output sig-rsa-key
            rkey1[2,3,4]  : root RSA public key for create keymax
            ukey1         : user RSA public key for create keymax
            skey          : root RSA private key for signature keymax
            output        : signatured keymax

    aml_encrypt_g12b --keybnd --ukey file --rootkeymax file [--aeskey file]
        --output file
            ukey       : user RSA key to be used for secure boot
            rootkeymax : signatured keymax for secure boot
            aeskey     : AES key to be used for secure boot
            output     : keys package for secure boot

    aml_encrypt_g12b --bootsig --input file --amluserkey file --output file
            input       : uboot image to be processed for normal or secure boot
            amluserkey  : amlogic key package for secure boot
            output      : signatured and encrypted uboot image

    aml_encrypt_g12b --imgsig --input file --amluserkey file --output file
            input      : image to be processed for secure boot
            amluserkey : amlogic key package for secure boot
            output     : signatured and encrypted kernel image

    aml_encrypt_g12b --binsig --input file --amluserkey file --output file
            input	   : binary file to be processed for secure boot
            amluserkey : amlogic key package for secure boot
            output	   : signatured and encrypted binary file

Note that the aml_encrypt_g12a and aml_encrypt_g12b executables appear to be
identical. Perhaps the executable name affects the behaviour?

"""

from binman.btool import _aml_common

# pylint: disable=C0103
class Bintoolaml_encrypt_g12b(_aml_common.aml_common):
    """Handles the 'aml_encrypt_g12b' tool

    This bintool supports running `aml_encrypt_g12b` to support creation of
    Amlogic images in binman.

    aml_encrypt_g12b provides a way to package firmware for Amlogic devices.

    It is also possible to fetch a binary version of the tool.
    """
    def __init__(self, name):
        super().__init__(name, 'Amlogic encrypt g12b')
