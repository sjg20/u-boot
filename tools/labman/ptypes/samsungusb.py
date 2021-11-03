# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import re
import time

import collections
from labman.ptypes.usbboot import Part_usbboot
from labman import work

class Part_samsungusb(Part_usbboot):
    """A Samsung exynos connection

    This allows downloading software to the DUT oevr USB using the boot ROM

    Properties:
        _symlink: Symlink to the device
    """
    def __init__(self):
        super().__init__()
        self.vendor = 0x04e8
        self.product = 0x1234
        self.recovery_method = self.Method.RECOVERY_RESET

    def load(self, yam):
        """Load the object from a yaml definition

        Args:
            yam (dict): Yaml definition
        """
        super().load(yam)
        self.bct = yam.get('bct')

    def __str__(self):
        """Convert the object into a string for presentation to the user"""
        return 'samsung %s' % self.name

    def raise_self(self, msg):
        """Raise an error related to this Samsung connection

        Args:
            msg (str): Message to report

        Raises:
            ValueError: always
        """
        raise ValueError('%s: %s' % (str(self), msg))
