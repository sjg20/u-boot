# SPDX-License-Identifier: GPL-2.0
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import os.path
import pytest

def test_spl(u_boot_console, ut_spl_subtest):
    """Execute a "ut" subtest.

    The subtests are collected in function generate_ut_subtest() from linker
    generated lists by applying a regular expression to the lines of file
    spl/u-boot-spl.sym. The list entries are created using the C macro
    UNIT_TEST().

    Strict naming conventions have to be followed to match the regular
    expression. Use UNIT_TEST(foo_test_bar, _flags, foo_test) for a test bar in
    test suite foo that can be executed via command 'ut foo bar' and is
    implemented in C function foo_test_bar().

    Args:
        u_boot_console (ConsoleBase): U-Boot console
        ut_subtest (str): test to be executed via command ut, e.g 'foo bar' to
            execute command 'ut foo bar'
    """
    cons = u_boot_console
    cons.restart_uboot_with_flags(['-u', ut_spl_subtest])
    output = cons.get_spawn_output().replace('\r', '')
    assert 'Failures: 0' in output
