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
    def setUp(self):
        # Create a temporary directory for test files
        self._indir = tempfile.mkdtemp(prefix='fip_util.')
        tools.SetInputDirs([self._indir])

        # Set up a temporary output directory, used by the tools library when
        # compressing files
        tools.PrepareOutputDir(None)

        self.have_fiptool = True
        try:
            tools.Run('which', 'fiptool2')
        except ValueError:
            self.have_fiptool = False

        self.src_file = os.path.join(self._indir, 'orig.py')
        self.outname = tools.GetOutputFilename('out.py')
        self.args = ['-D', '-s', self._indir, '-o', self.outname]
        self.readme = os.path.join(self._indir, 'readme.rst')
        self.macro_dir = os.path.join(self._indir, 'include/tools_share')
        self.macro_fname = os.path.join(self.macro_dir,
                                        'firmware_image_package.h')

    macro_contents = '''

/* ToC Entry UUIDs */
#define UUID_TRUSTED_UPDATE_FIRMWARE_SCP_BL2U \\
	{{0x65, 0x92, 0x27, 0x03}, {0x2f, 0x74}, {0xe6, 0x44}, 0x8d, 0xff, {0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10} }
#define UUID_TRUSTED_UPDATE_FIRMWARE_BL2U \\
	{{0x60, 0xb3, 0xeb, 0x37}, {0xc1, 0xe5}, {0xea, 0x41}, 0x9d, 0xf3, {0x19, 0xed, 0xa1, 0x1f, 0x68, 0x01} }

'''

    def tearDown(self):
        """Remove the temporary input directory and its contents"""
        if self._indir:
            shutil.rmtree(self._indir)
        self._indir = None
        tools.FinaliseOutputDir()

    def test_no_readme(self):
        """Test handling of a missing readme.rst"""
        with self.assertRaises(Exception) as err:
            fip_util.main(self.args, self.src_file)
        self.assertIn('Expected file', str(err.exception))

    def test_invalid_readme(self):
        """Test that an invalid readme.rst is detected"""
        tools.WriteFile(self.readme, 'blah', binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(self.args, self.src_file)
        self.assertIn('does not start with', str(err.exception))

    def setup_readme(self):
        tools.WriteFile(self.readme, 'Trusted Firmware-A\n==================',
                        binary=False)

    def setup_macro(self, data=macro_contents):
        os.makedirs(self.macro_dir)
        tools.WriteFile(self.macro_fname, data, binary=False)

    def test_no_fip_h(self):
        """Check handling of missing firmware_image_package.h"""
        self.setup_readme()
        with self.assertRaises(Exception) as err:
            fip_util.main(self.args, self.src_file)
        self.assertIn('No such file or directory', str(err.exception))

    def test_invalid_fip_h(self):
        """Check failure to parse firmware_image_package.h"""
        self.setup_readme()
        self.setup_macro('blah')
        with self.assertRaises(Exception) as err:
            fip_util.main(self.args, self.src_file)
        self.assertIn('Cannot parse file', str(err.exception))

    def test_parse_fip_h(self):
        """Check parsing of firmware_image_package.h"""
        self.setup_readme()
        # Check parsing the header file
        self.setup_macro()
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

    def test_rest(self):
        """Check parsing of the ATF source code"""
        self.setup_readme()
        self.setup_macro()

        # Still need the .c file
        with self.assertRaises(Exception) as err:
            fip_util.main(self.args, self.src_file)
        self.assertIn('tbbr_config.c', str(err.exception))

        # Check invalid format for C file
        name_dir = os.path.join(self._indir, 'tools/fiptool')
        name_fname = os.path.join(name_dir, 'tbbr_config.c')
        os.makedirs(name_dir)
        tools.WriteFile(name_fname, 'blah', binary=False)
        with self.assertRaises(Exception) as err:
            fip_util.main(self.args, self.src_file)
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

        # Check generating the file when changes are needed
        tools.WriteFile(self.src_file, '''

# This is taken from tbbr_config.c in ARM Trusted Firmware
FIP_TYPE_LIST = [
    # ToC Entry UUIDs
    FipType('scp-fwu-cfg', 'SCP Firmware Updater Configuration FWU SCP_BL2U',
            [0x65, 0x92, 0x27, 0x03, 0x2f, 0x74, 0xe6, 0x44,
             0x8d, 0xff, 0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10]),
    ] # end
blah blah
                        ''', binary=False)
        with test_util.capture_sys_output() as (stdout, _):
            fip_util.main(self.args, self.src_file)
        self.assertIn('Needs update', stdout.getvalue())

        # Check generating the file when no changes are needed
        tools.WriteFile(self.src_file, '''
# This is taken from tbbr_config.c in ARM Trusted Firmware
FIP_TYPE_LIST = [
    # ToC Entry UUIDs
    FipType('scp-fwu-cfg', 'SCP Firmware Updater Configuration FWU SCP_BL2U',
            [0x65, 0x92, 0x27, 0x03, 0x2f, 0x74, 0xe6, 0x44,
             0x8d, 0xff, 0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10]),
    FipType('ap-fwu-cfg', 'AP Firmware Updater Configuration BL2U',
            [0x60, 0xb3, 0xeb, 0x37, 0xc1, 0xe5, 0xea, 0x41,
             0x9d, 0xf3, 0x19, 0xed, 0xa1, 0x1f, 0x68, 0x01]),
    ] # end
blah blah''', binary=False)
        with test_util.capture_sys_output() as (stdout, _):
            fip_util.main(self.args, self.src_file)
        self.assertIn('is up-to-date', stdout.getvalue())


if __name__ == '__main__':
    unittest.main()
