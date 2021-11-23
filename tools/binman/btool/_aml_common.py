# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Common bintool implementation for aml_encrypt_g12a/b"""

import re

from binman import bintool

# pylint: disable=C0103
class aml_common(bintool.Bintool):
    """Common class for aml_encrypt_g12a/b tools"""
    def version(self):
        """Version handler

        Returns:
            str: Version number of tool
        """
        lines = self.run_cmd('', raise_on_error=False).strip().splitlines()
        if not lines:
            return super().version()
        out = lines[0]
        # AMLOGIC-G12A-G12B-SIG-module : Ver-0.07 Built on Nov  1 2018 17:12:32
        m_version = re.match(r'.*Ver-([^ ]*).*', out)
        return m_version.group(1).strip() if m_version else out

    def fetch(self, method):
        """Fetch handler for aml_encrypt_g12a/b

        This installs a binary version of this tool.

        Args:
            method (FETCH_...): Method to use

        Returns:
            True if the file was fetched, None if a method other than FETCH_BIN
            was requested

        Raises:
            Valuerror: Fetching could not be completed
        """
        if method != bintool.FETCH_BIN:
            return None
        fname, tmpdir = self.fetch_from_drive(
            '1o_OiFB5Lf9ib-nPuQDLub8OPuJ0mKIum')
        return fname, tmpdir
