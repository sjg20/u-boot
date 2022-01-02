# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Base class for all bintools

This defines the common functionality for all bintools, including running
the tool.
"""

import importlib

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
