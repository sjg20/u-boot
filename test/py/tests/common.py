# SPDX-License-Identifier:	GPL-2.0+
# Copyright 2022 Google LLC
#
# Common functions for tests

import os

import u_boot_utils as util

def make_fname(cons, leaf):
    """Make a temporary filename

    Args:
        leaf: Leaf name of file to create (within temporary directory)
    Return:
        Temporary filename
    """

    return os.path.join(cons.config.build_dir, leaf)

def make_its(cons, base_its, params, fname='test.its'):
    """Make a sample .its file with parameters embedded

    Args:
        params: Dictionary containing parameters to embed in the %() strings
    Returns:
        Filename of .its file created
    """
    its = make_fname(cons, fname)
    with open(its, 'w') as fd:
        print(base_its % params, file=fd)
    return its

def make_fit(cons, mkimage, base_its, params, fname='test.fit', base_fdt=None):
    """Make a sample .fit file ready for loading

    This creates a .its script with the selected parameters and uses mkimage to
    turn this into a .fit image.

    Args:
        mkimage: Filename of 'mkimage' utility
        params: Dictionary containing parameters to embed in the %() strings
    Return:
        Filename of .fit file created
    """
    fit = make_fname(cons, fname)
    its = make_its(cons, base_its, params)
    util.run_and_log(cons, [mkimage, '-f', its, fit])
    if base_fdt:
        with open(make_fname(cons, 'u-boot.dts'), 'w') as fd:
            fd.write(base_fdt)
    return fit

def make_kernel(cons, filename, text):
    """Make a sample kernel with test data

    Args:
        filename: the name of the file you want to create
    Returns:
        Full path and filename of the kernel it created
    """
    fname = make_fname(cons, filename)
    data = ''
    for i in range(100):
        data += 'this %s %d is unlikely to boot\n' % (text, i)
    with open(fname, 'w') as fd:
        print(data, file=fd)
    return fname

def make_dtb(cons, base_fdt):
    """Make a sample .dts file and compile it to a .dtb

    Returns:
        Filename of .dtb file created
    """
    src = make_fname(cons, 'u-boot.dts')
    dtb = make_fname(cons, 'u-boot.dtb')
    with open(src, 'w') as fd:
        fd.write(base_fdt)
    util.run_and_log(cons, ['dtc', src, '-O', 'dtb', '-o', dtb])
    return dtb
