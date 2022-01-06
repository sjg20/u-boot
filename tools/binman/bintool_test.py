# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#

"""Tests for the Bintool class"""

import unittest

from binman import bintool

class TestBintool(unittest.TestCase):
    """Tests for the Bintool class"""
    def test_missing_btype(self):
        """Test that unknown bintool types are detected"""
        with self.assertRaises(ValueError) as exc:
            bintool.Bintool.create('missing')
        self.assertIn("No module named 'binman.btool.missing'",
                      str(exc.exception))


if __name__ == "__main__":
    unittest.main()
