# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2022 Texas Instruments Incorporated - https://www.ti.com/
#
# Entry-type module for OP-TEE Trusted OS firmware blob
#

from binman.etype.blob_named_by_arg import Entry_blob_named_by_arg
from binman import elf

class Entry_tee_os(Entry_blob_named_by_arg):
    """Entry containing an OP-TEE Trusted OS (TEE) blob

    Properties / Entry arguments:
        - tee-os-path: Filename of file to read into entry. This is typically
            called tee.bin or tee.elf

    This entry holds the run-time firmware, typically started by U-Boot SPL.
    See the U-Boot README for your architecture or board for how to use it. See
    https://github.com/OP-TEE/optee_os for more information about OP-TEE.

    Note that if the file is in ELF format, it must go in a FIT. In that case,
    this entry will mark itself as not present.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node, 'tee-os')
        self.external = True

    def ObtainContents(self, fake_size=0):
        ok = super().ObtainContents(fake_size)
        if not ok:
            return False
        if not self.missing and elf.is_valid(self.data):
            self.mark_not_present('uses Elf format which must be in a FIT')
        return True
