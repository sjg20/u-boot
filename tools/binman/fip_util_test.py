#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>

"""Tests for fip_util

This tests a few features of fip_util which are not covered by binman's ftest.py
"""

import os
import shutil
import sys
import tempfile
import unittest

# Bring in the patman and dtoc libraries (but don't override the first path
# in PYTHONPATH)
OUR_PATH = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(2, os.path.join(OUR_PATH, '..'))

# pylint: disable=C0413
from patman import tools
import fip_util

class TestFip(unittest.TestCase):
    """Test of fip_util classes"""
    #pylint: disable=W0212
    @classmethod
    def setUpClass(cls):
        # Create a temporary directory for test files
        cls._indir = tempfile.mkdtemp(prefix='fip_util.')
        tools.SetInputDirs([cls._indir])

        # Set up a temporary output directory, used by the tools library when
        # compressing files
        tools.PrepareOutputDir(None)

        cls.have_fiptool = True
        try:
            tools.Run('which', 'fiptool2')
        except ValueError:
            cls.have_fiptool = False

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary input directory and its contents"""
        if cls._indir:
            shutil.rmtree(cls._indir)
        cls._indir = None
        tools.FinaliseOutputDir()

    def test_parse_atf_source(self):
        """Check parsing of the ATF source code"""
        fname = tools.GetOutputFilename('out.c')
        args = ['-s', self._indir, '-o', fname]
        with self.assertRaises(Exception) as err:
            fip_util.main(args)
        self.assertIn('Expected file', str(err.exception))

        readme = os.path.join(self._indir, 'readme.rst')
        tools.WriteFile(readme, 'blah', binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(args)
        self.assertIn('does not start with', str(err.exception))


if __name__ == '__main__':
    unittest.main()
