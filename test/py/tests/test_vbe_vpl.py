# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
# Test addition of VBE

import os

import pytest
import u_boot_utils

@pytest.mark.boardspec('sandbox_vpl')
@pytest.mark.requiredtool('dtc')
def test_vbe_vpl(u_boot_console):
    cons = u_boot_console

    # Set up the RAM buffer
    ram = os.path.join(cons.config.build_dir, 'ram.bin')

    # Make a copy of the FDT before making out changes
    fdt_in = os.path.join(cons.config.build_dir, 'arch/sandbox/dts/test.dtb')
    fdt = os.path.join(cons.config.build_dir, 'vpl_test.dtb')
    with open(fdt, 'wb') as outf:
        with open(fdt_in, 'rb') as inf:
            outf.write(inf.read())

    # Use the image file produced by binman
    image = os.path.join(cons.config.build_dir, 'image.bin')

    # Enable firmware1 and the mmc that it uses. These are needed for the full
    # VBE flow.
    u_boot_utils.run_and_log(
        cons, f'fdtput -t s {fdt} /bootstd/firmware0 status disabled')
    u_boot_utils.run_and_log(
        cons, f'fdtput -t s {fdt} /bootstd/firmware1 status okay')
    u_boot_utils.run_and_log(
        cons, f'fdtput -t s {fdt} /mmc3 status okay')
    u_boot_utils.run_and_log(
        cons, f'fdtput -t s {fdt} /mmc3 filename {image}')

    # Remove any existing RAM file, so we don't have old data present
    if os.path.exists(ram):
        os.remove(ram)
    try:
        cons.restart_uboot_with_flags(
            ['-p', image, '-w', '-s', 'state.dtb', '-d', fdt])

        # Make sure that VBE was used in both VPL (to load SPL) and SPL (to load
        # U-Boot
        output = cons.run_command('vbe state')
        assert output == 'Phases: VPL SPL'
    finally:
        # Restart afterward in case a non-VPL test is run next. This should not
        # happen since VPL tests are run in their own invocation of test.py, but
        # the cost of doing this is not too great at present.
        u_boot_console.restart_uboot()
