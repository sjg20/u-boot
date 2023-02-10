     # SPDX-License-Identifier: GPL-2.0
# Copyright 2023 Google LLC
# Written by Simon Glass <sjg@chromium.org>

"""Test handling of unmigrated u-boot,dm- tags"""

import os
import pytest

import u_boot_utils as util

# This is needed for Azure, since the default '..' directory is not writeable
TMPDIR = '/tmp/test_kconfig'

@pytest.mark.slow
@pytest.mark.boardspec('sandbox')
def test_of_migrate(u_boot_console):
    """Test sandbox with old boot phase tags like u-boot,dm-..."""
    cons = u_boot_console

    srcdir = cons.config.source_dir
    build_dir = cons.config.build_dir

    dt_dir = os.path.join(build_dir, 'arch', 'sandbox', 'dts')
    dtb_fname = os.path.join(dt_dir, 'sandbox.dtb')
    out_dts = os.path.join(dt_dir, 'sandbox_out.dts')
    util.run_and_log(cons, ['dtc', dtb_fname, '-I', 'dtb', '-O', 'dts',
                            '-o', out_dts])

    with open(out_dts) as inf:
        data = inf.read()

    dts_fname = os.path.join(dt_dir, 'sandbox_new.dts')
    with open(dts_fname, 'w') as outf:
        print(data, file=outf)
    new_dtb = os.path.join(dts_dir, 'sandbox_new.dtb')
    util.run_and_log(cons, ['dtc', dts_fname, '-o', new_dtb])

    env = dict(os.environ)
    env['EXT_DTB'] = dtb_fname
    env['DEVICE_TREE'] = 'sandbox_new'
    env['NO_LTO'] = '1'
    out = util.run_and_log(
        cons, ['./tools/buildman/buildman', '-m', '--board', 'sandbox',
               '-o', TMPDIR], ignore_errors=True, env=env)
    assert 'invalid_use_of_IF_ENABLED_INT' in out
    assert 'invalid_use_of_CONFIG_IF_ENABLED_INT' in out

''''
@pytest.mark.slow
@pytest.mark.boardspec('sandbox_spl')
def test_kconfig_spl(u_boot_console):
    """Test build failures when IF_ENABLED_INT() option is not enabled"""
    cons = u_boot_console

    # This detects build errors in test/lib/kconfig_spl.c
    out = util.run_and_log(
        cons, ['./tools/buildman/buildman', '-m', '--board', 'sandbox_spl',
               '-a', 'TEST_KCONFIG', '-o', TMPDIR], ignore_errors=True)
    assert 'invalid_use_of_IF_ENABLED_INT' in out

    # There is no CONFIG_SPL_TEST_KCONFIG, so the CONFIG_IF_ENABLED_INT()
    # line should not generate an error
    assert 'invalid_use_of_CONFIG_IF_ENABLED_INT' not in out
'''

