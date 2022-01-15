# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#

"""Utility functions for dealing with Kconfig .confing files"""

import re

from patman import tools

def adjust_cfg_lines(lines, adjust_cfg):
    re_line = re.compile(r'CONFIG_([A-Z0-9_]+)=(.*)')
    out_lines = []
    for line in lines:
        out_line = line
        m_line = re_line.match(line)
        opt, val = m_line.groups()
        adj = adjust_cfg.get(opt)
        print('adj', adj)
        if adj:
            if adj[0] == '~':
                out_line = f'# CONFIG_{opt} is not set'

        out_lines.append(out_line)
    return out_lines

def adjust_cfg_file(fname, adjust_cfg):
    """Make adjustments to a .config file

    Args:
        fname (str): Filename of .config file to change
        adjust_cfg (list of str): List of changes to make to .config file
            before building. Each is one of (where C is either CONFIG_xxx
            or just xxx):
                 C to enable C
                 ~C to disable C
                 C=val to set the value of C (val must have quotes if C is
                     a string Kconfig
    """
    lines = tools.ReadFile(fname, binary=False).splitlines()
    out_lines = adjust_cfg_lines(lines, adjust_cfg)
    out = '\n'.join(out_lines) + '\n'
    tools.WriteFile(fname, out, binary=False)
