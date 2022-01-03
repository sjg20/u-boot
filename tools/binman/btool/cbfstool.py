# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for cbfstool"""

from binman import bintool
from patman import tools

class Bintoolcbfstool(bintool.Bintool):
    """Handles the 'cbfstool' tool"""
    def __init__(self, name):
        super().__init__(name)
        self.toolname = 'cbfstool'
        self.desc = 'Manipulate CBFS files'
