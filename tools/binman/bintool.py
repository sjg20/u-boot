# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Base class for all bintools

This defines the common functionality for all bintools, including running
the tool.
"""

import glob
import importlib
import multiprocessing
import os
import shutil
import tempfile

from patman import terminal
from patman import tools

BINMAN_DIR = os.path.dirname(os.path.realpath(__file__))
FORMAT = '%-15.15s %-12.12s %-26.26s %s'

modules = {}

FETCH_ANY, FETCH_BIN, FETCH_BUILD = range(3)

FETCH_NAMES = {
    FETCH_ANY: 'any method',
    FETCH_BIN: 'binary download',
    FETCH_BUILD: 'build from source'
    }

class Bintool:
    """Tool which operates on binaries to help produce entry contents

    This is the base class for all bintools
    """
    def __init__(self, name, desc):
        self.name = name
        self.desc = desc

    @staticmethod
    def find_bintool_class(btype):
        """Look up the bintool class for bintool

        Args:
            byte: Bintool to use, e.g. 'mkimage'

        Returns:
            The bintool class object if found, else a tuple:
                module name that could not be found
                exception received
        """
        # Convert something like 'u-boot' to 'u_boot' since we are only
        # interested in the type.
        module_name = btype.replace('-', '_')
        module = modules.get(module_name)

        # Import the module if we have not already done so
        if not module:
            try:
                module = importlib.import_module('binman.btool.' + module_name)
            except ImportError as exc:
                return module_name, exc
            modules[module_name] = module

        # Look up the expected class name
        return getattr(module, 'Bintool%s' % module_name)

    @staticmethod
    def create(name):
        """Create a new bintool object

        Args:
            name (str): Bintool to create, e.g. 'mkimage'

        Returns:
            A new object of the correct type (a subclass of Binutil)
        """
        cls = Bintool.find_bintool_class(name)
        if isinstance(cls, tuple):
            raise ValueError("Cannot import bintool module '%s': %s" % cls)

        # Call its constructor to get the object we want.
        obj = cls(name)
        return obj

    def show(self):
        """Show a line of information about a bintool"""
        if self.is_present():
            version = self.version()
        else:
            version = '-'
        print(FORMAT % (self.name, version, self.desc,
                        self.get_path() or '(not found)'))

    @staticmethod
    def get_tool_list():
        files = glob.glob(os.path.join(BINMAN_DIR, 'btool/*'))
        names = [os.path.splitext(os.path.basename(fname))[0]
                 for fname in files]
        return sorted(names)

    @staticmethod
    def list_all():
        """List all the bintools known to binman"""
        names = Bintool.get_tool_list()
        print(FORMAT % ('Name', 'Version', 'Description', 'Path'))
        print(FORMAT % ('-' * 15,'-' * 12, '-' * 25, '-' * 30))
        for name in names:
            btool = Bintool.create(name)
            btool.show()

    def is_present(self):
        """Check if a bintool is available on the system

        Returns:
            bool: True if available, False if not
        """
        return bool(self.get_path())

    def get_path(self):
        """Get the path of a bintool

        Returns:
            str: Path to the tool, if available, else None
        """
        return tools.tool_find(self.name)

    @staticmethod
    def fetch_tools(method, names_to_fetch):
        col = terminal.Color()
        if len(names_to_fetch) == 1 and names_to_fetch[0] == 'all':
            name_list = Bintool.get_tool_list()
            print(col.Color(col.YELLOW,
                            'Fetching tools: %s' % ' '.join(name_list)))
        else:
            name_list = names_to_fetch
        fetched = 0
        fail = 0
        for name in name_list:
            print(col.Color(col.YELLOW, 'Fetch: %s' % name))
            btool = Bintool.create(name)
            if method == FETCH_ANY:
                for try_method in FETCH_BIN, FETCH_BUILD:
                    print(f'- trying method: {FETCH_NAMES[try_method]}')
                    result = btool.fetch(try_method)
                    if result:
                        break
            else:
                result = btool.fetch(method)
            if result:
                fetched += 1
                if result != True:
                    fname, tmpdir = result
                    dest = os.path.join(os.getenv('HOME'), 'bin', name)
                    print(f"- writing to '{dest}'")
                    tools.Run('mv', fname, dest)
                    if tmpdir:
                        shutil.rmtree(tmpdir)
            elif not result:
                fail += 1
                if method == FETCH_ANY:
                    print('- failed to fetch with all methods')
                else:
                    print(f"- method '{FETCH_NAMES[method]}' is not supported")
        if names_to_fetch[0] == 'all':
            print(col.Color(col.RED if fail else col.GREEN,
                            f'Tools fetched: {fetched}, failures {fail}'))

    def fetch(self, method):
        print(f"No method to fetch bintool '{self.name}'")

    def version(self):
        return 'unknown'

    def run_cmd(self, *args):
        return tools.Run(self.name, *args)

    def build_from_git(self, git_repo, make_target, bintool_path):
        """Build a tool from a git repo
        """
        tmpdir = tempfile.mkdtemp(prefix='binmanf.')
        print(f"- clone git repo '{git_repo}' to '{tmpdir}'")
        tools.Run('git', 'clone', '--depth', '1', git_repo, tmpdir)
        print(f"- build target '{make_target}'")
        tools.Run('make', '-C', tmpdir, '-j', f'{multiprocessing.cpu_count()}',
                  make_target)
        fname = os.path.join(tmpdir, bintool_path)
        if not os.path.exists(fname):
            print(f"- File '{fname}' was not produced")
            return None
        return fname, tmpdir

    def fetch_from_url(self, url):
        fname, tmpdir = tools.Download(url)
        tools.Run('chmod', 'a+x', fname)
        return fname, tmpdir

    def fetch_from_drive(self, drive_id):
        url = f'https://drive.google.com/uc?export=download&id={drive_id}'
        return self.fetch_from_url(url)

    def apt_install(self, package):
        args = ['sudo', 'apt', 'install', '-y', package]
        print('- %s' % ' '.join(args))
        tools.Run(*args)
        return True
