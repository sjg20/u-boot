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
from patman import test_util
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
        # no readme.txt
        self.maxDiff = None
        src_file = os.path.join(self._indir, 'orig.py')
        fname = tools.GetOutputFilename('out.py')
        args = ['-D', '-s', self._indir, '-o', fname]
        with self.assertRaises(Exception) as err:
            fip_util.main(args, src_file)
        self.assertIn('Expected file', str(err.exception))

        # Invalid header for readme.txt
        readme = os.path.join(self._indir, 'readme.rst')
        tools.WriteFile(readme, 'blah', binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(args, src_file)
        self.assertIn('does not start with', str(err.exception))

        # No firmware_image_package.h
        readme = os.path.join(self._indir, 'readme.rst')
        tools.WriteFile(readme, 'Trusted Firmware-A\n==================',
                        binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(args, src_file)
        self.assertIn('No such file or directory', str(err.exception))

        # Invalid format for firmware_image_package.h
        macro_dir = os.path.join(self._indir, 'include/tools_share')
        macro_fname = os.path.join(macro_dir, 'firmware_image_package.h')
        os.makedirs(macro_dir)
        tools.WriteFile(macro_fname, 'blah', binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(args, src_file)
        self.assertIn('Cannot parse file', str(err.exception))

        # Check parsing the header file
        tools.WriteFile(macro_fname, '''

/* ToC Entry UUIDs */
#define UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U \\
	{{0x65, 0x92, 0x27, 0x03}, {0x2f, 0x74}, {0xe6, 0x44}, 0x8d, 0xff, {0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10} }
#define UUID_TRUSTED_UPDATE_FIRMWARE_BL2U \\
	{{0x60, 0xb3, 0xeb, 0x37}, {0xc1, 0xe5}, {0xea, 0x41}, 0x9d, 0xf3, {0x19, 0xed, 0xa1, 0x1f, 0x68, 0x01} }

''', binary=False)
        macros = fip_util.parse_macros(self._indir)
        expected_macros = {
            'UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U':
                ('ToC Entry UUIDs', 'UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U',
                 bytes([0x65, 0x92, 0x27, 0x03, 0x2f, 0x74, 0xe6, 0x44,
                        0x8d, 0xff, 0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10])),
            'UUID_TRUSTED_UPDATE_FIRMWARE_BL2U':
                ('ToC Entry UUIDs', 'UUID_TRUSTED_UPDATE_FIRMWARE_BL2U',
                 bytes([0x60, 0xb3, 0xeb, 0x37, 0xc1, 0xe5, 0xea, 0x41,
                        0x9d, 0xf3, 0x19, 0xed, 0xa1, 0x1f, 0x68, 0x01])),
            }
        self.assertEqual(expected_macros, macros)

        # Still need the .c file
        with self.assertRaises(Exception) as err:
            fip_util.main(args, src_file)
        self.assertIn('tbbr_config.c', str(err.exception))

        # Check invalid format for C file
        name_dir = os.path.join(self._indir, 'tools/fiptool')
        name_fname = os.path.join(name_dir, 'tbbr_config.c')
        os.makedirs(name_dir)
        tools.WriteFile(name_fname, 'blah', binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(args, src_file)
        self.assertIn('Cannot parse file', str(err.exception))

        # Check parsing the C file
        tools.WriteFile(name_fname, '''

toc_entry_t toc_entries[] = {
	{
		.name = "SCP Firmware Updater Configuration FWU SCP_BL2U",
		.uuid = UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U,
		.cmdline_name = "scp-fwu-cfg"
	},
	{
		.name = "AP Firmware Updater Configuration BL2U",
		.uuid = UUID_TRUSTED_UPDATE_FIRMWARE_BL2U,
		.cmdline_name = "ap-fwu-cfg"
	},
''', binary=False)
        names = fip_util.parse_names(self._indir)

        expected_names = {
            'UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U': (
                'SCP Firmware Updater Configuration FWU SCP_BL2U',
                 'UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U',
                 'scp-fwu-cfg'),
            'UUID_TRUSTED_UPDATE_FIRMWARE_BL2U': (
                'AP Firmware Updater Configuration BL2U',
                'UUID_TRUSTED_UPDATE_FIRMWARE_BL2U',
                'ap-fwu-cfg'),
            }
        self.assertEqual(expected_names, names)

        # Check generating the file
        tools.WriteFile(src_file, '', binary=False)
        with test_util.capture_sys_output() as (stdout, stderr):
            fip_util.main(args, src_file)
        self.assertIn('Needs update', stdout.getvalue())

        fname


if __name__ == '__main__':
    unittest.main()
