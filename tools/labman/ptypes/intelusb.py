# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

from labman.ptypes.usbboot import Part_usbboot
from labman import work

class Part_intelusb(Part_usbboot):
    """A DFU (Device Firmware Update) connection for Intel Edison

    Properties:
    """
    def __init__(self):
        super().__init__()
        self.vendor = 0x8087
        self.product = 0x0a9e
        self.recovery_method = self.Method.RECOVERY_POWER_EXTRA

    def __str__(self):
        """Convert the object into a string for presentation to the user"""
        return 'dfu %s' % self.name

    def get_detail(self, port=None):
        return '%s, dfu' % self._symlink
