# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Base class for all bintools

This defines the common functionality for all bintools, including running
the tool.
"""

import glob
import importlib
import os

from patman import tools

BINMAN_DIR = os.path.dirname(os.path.realpath(__file__))
FORMAT = '%-15.15s %-12.12s %-26.26s %s'

modules = {}

class Bintool:
    """Tool which operates on binaries to help produce entry contents

    This is the base class for all bintools
    """
    def __init__(self, name):
        self.name = name

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

        # Call its constructor to get the object we want.
        obj = cls(name)
        return obj

    def show(self):
        if self.get_status() == 'OK':
            version = self.version()
        else:
            version = '-'
        print(FORMAT % (self.toolname, version, self.desc,
                        self.get_path() or '(not found)'))

    @staticmethod
    def list_all():
        files = glob.glob(os.path.join(BINMAN_DIR, 'btool/*'))
        print(FORMAT % ('Name', 'Version', 'Description', 'Path'))
        print(FORMAT % ('-' * 15,'-' * 12, '-' * 25, '-' * 30))
        for fname in files:
            name = os.path.splitext(os.path.basename(fname))[0]
            btool = Bintool.create(name)
            btool.show()

    def get_status(self):
        return 'OK' if self.get_path() else 'missing'

    def get_path(self):
        return tools.tool_find(self.name)

    @staticmethod
    def fetch_list(name_list):
        for name in name_list:
            print('Fetch: %s' % name)
            btool = Bintool.create(name)
            btool.fetch()

    def fetch(self):
        print(f"No method to fetch bintool '{self.name}'")

    def version(self):
        return 'unknown'

    def run_cmd(self, *args):
        return tools.Run(self.toolname, *args)
