# SPDX-License-Identifier: GPL-2.0+
# Copyright 2023 Google LLC
#

"""Build a FIT containing a lot of devicetree files"""
import argparse
import os
import sys
import time

import libfdt
import lz4.frame

def parse_args():
    epilog = 'Build a FIT from a directory tree containing .dtb files'
    parser = argparse.ArgumentParser(epilog=epilog)
    parser.add_argument('-f', '--fit', type=str, required=True,
          help='Specifies the output file (.fit)')
    parser.add_argument('srcdir', type=str, nargs='*',
          help='Specifies the directory tree that contains .dtb files')

    return parser.parse_args()

def setup_fit(fsw):
    fsw.INC_SIZE = 65536
    fsw.finish_reservemap()
    fsw.begin_node('')
    fsw.property_string('description', 'DTB set')
    fsw.property_u32('#address-cells', 1)

    fsw.property_u32('timestamp', int(time.time()))
    fsw.begin_node('images')
    with fsw.add_node('kernel'):
        fsw.property_string('description', 'dummy kernel')
        fsw.property_string('type', 'kernel_noload')
        fsw.property_string('arch', 'arm')
        fsw.property_string('os', 'Linux')
        fsw.property_string('compression', 'none')
        fsw.property_string('data', 'abcd' * 10)
        fsw.property_u32('load', 0)
        fsw.property_u32('entry', 0)

def finish_fit(fsw, entries):
    fsw.end_node()
    seq = 0
    with fsw.add_node('configurations'):
        for model, compat in entries:
            seq += 1
            with fsw.add_node(f'conf-{seq}'):
                fsw.property('compatible', bytes(compat))
                fsw.property_string('description', model)
                fsw.property_string('fdt', f'fdt-{seq}')
                fsw.property_string('kernel', 'kernel')
    fsw.end_node()

def output_dtb(fsw, seq, dtb_fname, data):
    with fsw.add_node(f'fdt-{seq}'):
        fdt = libfdt.FdtRo(data)
        model = fdt.getprop(0, 'model').as_str()
        compat = fdt.getprop(0, 'compatible')
        fsw.property_string('description', model)
        fsw.property_string('type', 'flat_dt')
        fsw.property_string('arch', 'arm')
        fsw.property_string('compression', 'lz4')
        fsw.property('compatible', bytes(compat))
        compressed = lz4.frame.compress(data)
        fsw.property('data', compressed)
    return model, compat


def run_make_fit():
    args = parse_args()

    fsw = libfdt.FdtSw()
    setup_fit(fsw)
    seq = 0
    size = 0
    entries = []
    for path in args.srcdir:
        for dirpath, _, fnames in os.walk(path):
            for fname in fnames:
                with open(os.path.join(dirpath, fname), 'rb') as inf:
                    seq += 1
                    data = inf.read()
                    size += len(data)
                    model, compat = output_dtb(fsw, seq, fname, data)
                    entries.append([model, compat])

    # Hack for testing on sandbox
    model, compat = output_dtb(fsw, seq + 1, fname, data)
    entries.append(['U-Boot sandbox', b'sandbox\0'])

    finish_fit(fsw, entries)
    fdt = fsw.as_fdt()
    out_data = fdt.as_bytearray()
    with open(args.fit, 'wb') as outf:
        outf.write(out_data)
    comp_size = len(out_data)
    print(f'Fit size {comp_size:x} / {comp_size / 1024 / 1024:.1f} MB', end='')
    print(f', {seq} files, uncompressed {size / 1024 / 1024:.1f} MB')


if __name__ == "__main__":
    sys.exit(run_make_fit())
