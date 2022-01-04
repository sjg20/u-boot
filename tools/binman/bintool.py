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
import tempfile

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
    def list_all():
        """List all the bintools known to binman"""
        files = glob.glob(os.path.join(BINMAN_DIR, 'btool/*'))
        print(FORMAT % ('Name', 'Version', 'Description', 'Path'))
        print(FORMAT % ('-' * 15,'-' * 12, '-' * 25, '-' * 30))
        for fname in files:
            name = os.path.splitext(os.path.basename(fname))[0]
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
    def fetch_tools(method, name_list):
        for name in name_list:
            print('Fetch: %s' % name)
            btool = Bintool.create(name)
            if method == FETCH_ANY:
                for try_method in FETCH_BIN, FETCH_BUILD:
                    print(f'- trying method: {FETCH_NAMES[try_method]}')
                    fname = btool.fetch(try_method)
                    if fname:
                        break
            else:
                fname = btool.fetch(method)
            if fname:
                dest = os.path.join(os.getenv('HOME'), 'bin', name)
                print(f"- writing to '{dest}'")
                tools.Run('mv', fname, dest)
            else:
                if method == FETCH_ANY:
                    print('- failed to fetch with all methods')
                else:
                    print(f"- method '{FETCH_NAMES[method]}' is not supported")

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
        return fname
