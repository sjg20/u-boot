# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#

"""Utility functions for dealing with Kconfig .confing files"""

import re

from patman import tools

RE_LINE = re.compile(r'(# )?CONFIG_([A-Z0-9_]+)(=(.*)| is not set)')

def make_cfg_line(opt, adj):
    if adj[0] == '~':
        return f'# CONFIG_{opt} is not set'
    else:
        return f'CONFIG_{opt}=1'

def adjust_cfg_line(line, adjust_cfg, done=None):
    out_line = line
    m_line = RE_LINE.match(line)
    comment, opt, right, val = m_line.groups()
    adj = adjust_cfg.get(opt)
    #print('adj', adj, right, val)
    if adj:
        out_line = make_cfg_line(opt, adj)
        if done is not None:
            done.add(opt)

    return out_line

def adjust_cfg_lines(lines, adjust_cfg):
    out_lines = []
    done = set()
    for line in lines:
        out_line = adjust_cfg_line(line, adjust_cfg, done)
        out_lines.append(out_line)

    for opt, val in adjust_cfg.items():
        if opt not in done:
            adj = adjust_cfg.get(opt)
            out_line = make_cfg_line(opt, adj)
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
