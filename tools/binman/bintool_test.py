# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#

"""Tests for the Bintool class"""

import collections
import os
import shutil
import tempfile
import unittest
import urllib.error

from binman import bintool
from binman.bintool import Bintool

from patman import terminal
from patman import test_util
from patman import tools

class TestBintool(unittest.TestCase):
    """Tests for the Bintool class"""
    @classmethod
    def setUpClass(cls):
        # Create a temporary directory for test files
        cls._indir = tempfile.mkdtemp(prefix='bintool.')

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary input directory and its contents"""
        #if cls._indir:
            #shutil.rmtree(cls._indir)
        cls._indir = None

    def test_missing_btype(self):
        """Test that unknown bintool types are detected"""
        with self.assertRaises(ValueError) as exc:
            Bintool.create('missing')
        self.assertIn("No module named 'binman.btool.missing'",
                      str(exc.exception))

    def test_fresh_bintool(self):
        """Check that the _testing bintool is not cached"""
        btest = Bintool.create('_testing')
        btest.present = True
        btest2 = Bintool.create('_testing')
        self.assertFalse(btest2.present)

    def test_version(self):
        """Check handling of a tool being present or absent"""
        btest = Bintool.create('_testing')
        with test_util.capture_sys_output() as (stdout, _):
            btest.show()
        self.assertFalse(btest.is_present())
        self.assertIn('-', stdout.getvalue())
        btest.present = True
        self.assertTrue(btest.is_present())
        self.assertEqual('123', btest.version())
        with test_util.capture_sys_output() as (stdout, _):
            btest.show()
        self.assertIn('123', stdout.getvalue())

    def test_fetch_present(self):
        """Test fetching of a tool"""
        btest = Bintool.create('_testing')
        btest.present = True
        col = terminal.Color()
        self.assertEqual(bintool.PRESENT,
                         btest.fetch_tool(bintool.FETCH_ANY, col, True))

    def test_fetch_url_err(self):
        """Test an error while fetching a tool from a URL"""
        def fail_download(url):
            """Take the tools.Download() function by raising an exception"""
            raise urllib.error.URLError('my error')

        btest = Bintool.create('_testing')
        col = terminal.Color()
        with unittest.mock.patch.object(tools, 'Download',
                                        side_effect=fail_download):
            with test_util.capture_sys_output() as (stdout, _):
                btest.fetch_tool(bintool.FETCH_ANY, col, False)
        self.assertIn('my error', stdout.getvalue())

    def test_fetch_url_exception(self):
        """Test an exception while fetching a tool from a URL"""
        def cause_exc(url):
            raise ValueError('exc error')

        btest = Bintool.create('_testing')
        col = terminal.Color()
        with unittest.mock.patch.object(tools, 'Download',
                                        side_effect=cause_exc):
            with test_util.capture_sys_output() as (stdout, _):
                btest.fetch_tool(bintool.FETCH_ANY, col, False)
        self.assertIn('exc error', stdout.getvalue())

    def test_fetch_method(self):
        """Test fetching using a particular method"""
        def fail_download(url):
            """Take the tools.Download() function by raising an exception"""
            raise urllib.error.URLError('my error')

        btest = Bintool.create('_testing')
        col = terminal.Color()
        with unittest.mock.patch.object(tools, 'Download',
                                        side_effect=fail_download):
            with test_util.capture_sys_output() as (stdout, _):
                btest.fetch_tool(bintool.FETCH_BIN, col, False)
        self.assertIn('my error', stdout.getvalue())

    def test_fetch_pass_fail(self):
        """Test fetching multiple tools with some passing and some failing"""
        def handle_download(url):
            """Take the tools.Download() function by writing a file"""
            if self.seq:
                raise urllib.error.URLError('not found')
            self.seq += 1
            tools.WriteFile(fname, expected)
            return fname, dirname

        expected = b'this is a test'
        dirname = os.path.join(self._indir, 'download_dir')
        os.mkdir(dirname)
        fname = os.path.join(dirname, 'downloaded')
        destdir = os.path.join(self._indir, 'dest_dir')
        os.mkdir(destdir)
        dest_fname = os.path.join(destdir, '_testing')
        self.seq = 0

        with unittest.mock.patch.object(bintool, 'DOWNLOAD_DESTDIR', destdir):
            with unittest.mock.patch.object(tools, 'Download',
                                            side_effect=handle_download):
                with test_util.capture_sys_output() as (stdout, _):
                    Bintool.fetch_tools(bintool.FETCH_ANY, ['_testing'] * 2)
        self.assertTrue(os.path.exists(dest_fname))
        data = tools.ReadFile(dest_fname)
        self.assertEqual(expected, data)

        lines = stdout.getvalue().splitlines()
        self.assertTrue(len(lines) > 2)
        self.assertEqual('Tools fetched:    1: _testing', lines[-2])
        self.assertEqual('Failures:         1: _testing', lines[-1])

    def test_tool_list(self):
        self.assertGreater(len(Bintool.get_tool_list()), 5)

    def check_fetch_all(self, method):
        def fake_fetch(method, col, skip_present):
            """Fakes the Binutils.fetch() function

            Returns FETCHED and FAIL on alternate calls
            """
            self.seq += 1
            result = bintool.FETCHED if self.seq & 1 else bintool.FAIL
            self.count[result] += 1
            return result

        self.seq = 0
        self.count = collections.defaultdict(int)
        with unittest.mock.patch.object(bintool.Bintool, 'fetch_tool',
                                        side_effect=fake_fetch):
            with test_util.capture_sys_output() as (stdout, _):
                Bintool.fetch_tools(method, ['all'])
        lines = stdout.getvalue().splitlines()
        self.assertIn(f'{self.count[bintool.FETCHED]}: ', lines[-2])
        self.assertIn(f'{self.count[bintool.FAIL]}: ', lines[-1])

    def test_fetch_all(self):
        """Test fetching all tools"""
        self.check_fetch_all(bintool.FETCH_ANY)

    def test_fetch_all_specific(self):
        """Test fetching all tools with a specific method"""
        self.check_fetch_all(bintool.FETCH_BIN)

    def test_fetch_missing(self):
        """Test fetching missing tools"""
        def fake_fetch2(method, col, skip_present):
            """Fakes the Binutils.fetch() function

            Returns PRESENT only for the '_testing' bintool
            """
            btool = list(self.btools.values())[self.seq]
            self.seq += 1
            print('fetch', btool.name)
            if btool.name == '_testing':
                return bintool.PRESENT
            return bintool.FETCHED

        # Preload a list of tools to return when get_tool_list() and create()
        # are called
        all_tools = Bintool.get_tool_list() + ['_testing']
        self.btools = collections.OrderedDict()
        for name in all_tools:
            self.btools[name] = Bintool.create(name)
        self.seq = 0
        with unittest.mock.patch.object(bintool.Bintool, 'fetch_tool',
                                        side_effect=fake_fetch2):
            with unittest.mock.patch.object(bintool.Bintool,
                                            'get_tool_list',
                                            side_effect=[all_tools]):
                with unittest.mock.patch.object(bintool.Bintool, 'create',
                                                side_effect=self.btools.values()):
                    with test_util.capture_sys_output() as (stdout, _):
                        Bintool.fetch_tools(bintool.FETCH_ANY, ['missing'])
        lines = stdout.getvalue().splitlines()
        num_tools = len(self.btools)
        fetched = [line for line in lines if 'Tools fetched:' in line].pop()
        present = [line for line in lines if 'Already present:' in line].pop()
        self.assertIn(f'{num_tools - 1}: ', fetched)
        self.assertIn(f'1: ', present)

    def check_build_method(self, write_file):
        def fake_run(*cmd):
            if cmd[0] == 'make':
                # See Bintool.build_from_git()
                tmpdir = cmd[2]
                self.fname = os.path.join(tmpdir, 'pathname')
                if write_file:
                    tools.WriteFile(self.fname, b'hello')

        btest = Bintool.create('_testing')
        col = terminal.Color()
        self.fname = None
        with unittest.mock.patch.object(bintool, 'DOWNLOAD_DESTDIR',
                                        self._indir):
            with unittest.mock.patch.object(tools, 'Run', side_effect=fake_run):
                with test_util.capture_sys_output() as (stdout, _):
                    btest.fetch_tool(bintool.FETCH_BUILD, col, False)
        fname = os.path.join(self._indir, '_testing')
        return fname if write_file else self.fname, stdout.getvalue()

    def test_build_method(self):
        """Test fetching using the build method"""
        fname, stdout = self.check_build_method(write_file=True)
        self.assertTrue(os.path.exists(fname))
        self.assertIn(f"writing to '{fname}", stdout)

    def test_build_method_fail(self):
        """Test fetching using the build method when no file is produced"""
        fname, stdout = self.check_build_method(write_file=False)
        self.assertFalse(os.path.exists(fname))
        self.assertIn(f"File '{fname}' was not produced", stdout)


if __name__ == "__main__":
    unittest.main()
