# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool used for testing

This is not a real bintool, just one used for testing"""

from binman import bintool

class Bintool_testing(bintool.Bintool):
    """Bintool used for testing"""
    def __init__(self, name):
        super().__init__(name, 'testing')
        self.present = False

    def is_present(self):
        return self.present

    def version(self):
        return '123'

    def fetch(self, method):
        if method == bintool.FETCH_BIN:
            return self.fetch_from_drive('junk')
            return fname, tmpdir
        elif method == bintool.FETCH_BUILD:
            return self.build_from_git('url', 'target', 'pathname')
