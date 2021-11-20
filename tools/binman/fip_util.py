#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>

"""Support for ARM's Firmware Image Package (FIP) format

FIP is a format similar to FMAP[1] but with fewer features and an obscure UUID
instead of the region name.

It consists of a header and a table of entries, each pointing to a place in the
firmware image where something can be found.

[1] https://chromium.googlesource.com/chromiumos/third_party/flashmap/+/refs/heads/master/lib/fmap.h
"""

from argparse import ArgumentParser
import collections
import io
import os
import re
import struct
import sys

# Bring in the patman and dtoc libraries (but don't override the first path
# in PYTHONPATH)
OUR_FILE = os.path.realpath(__file__)
OUR_PATH = os.path.dirname(OUR_FILE)
sys.path.insert(2, os.path.join(OUR_PATH, '..'))


from patman import tools

# The TOC header, at the start of the FIP
HEADER_FORMAT   = '<IIQ'
HEADER_LEN      = 0x10
HEADER_MAGIC    = 0xaA640001
HEADER_SERIAL   = 0x12345678

# The entry header (a table of these comes after the TOC header)
UUID_LEN        = 16
ENTRY_FORMAT    = '<%dsQQQ' % UUID_LEN
ENTRY_SIZE      = 0x28

HEADER_NAMES = (
    'name',
    'serial',
    'flags',
)

ENTRY_NAMES = (
    'uuid',
    'offset_addr',
    'size',
    'flags',
)

class FipType:
    def __init__(self, name, desc, uuid_bytes):
        self.name = name
        self.desc = desc
        self.uuid = bytes(uuid_bytes)

# This is taken from tbbr_config.c in ARM Trusted Firmware
FIP_TYPE_LIST = [
    # ToC Entry UUIDs
    FipType('scp-fwu-cfg', 'SCP Firmware Updater Configuration FWU SCP_BL2U',
            [0x65, 0x92, 0x27, 0x03, 0x2f, 0x74, 0xe6, 0x44,
             0x8d, 0xff, 0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10]),
    FipType('soc-fw', 'EL3 Runtime Firmware BL31',
            [0x47, 0xd4, 0x08, 0x6d, 0x4c, 0xfe, 0x98, 0x46,
             0x9b, 0x95, 0x29, 0x50, 0xcb, 0xbd, 0x5a, 0x00]),
    ] # end

FIP_TYPES = {ftype.name: ftype for ftype in FIP_TYPE_LIST}


def align_int(val, align):
    """Align a value up to the given alignment

    Args:
        val: Integer value to align
        align: Integer alignment value (e.g. 4 to align to 4-byte boundary)

    Returns:
        integer value aligned to the required boundary, rounding up if necessary
    """
    return int((val + align - 1) / align) * align

class FipHeader:
    def __init__(self, name, serial, flags):
        self.name = name
        self.serial = serial
        self.flags = flags


class FipEntry:
    """Class to represent a single FIP entry

    This is used to hold the information about an entry, including its contents.
    Use the get_data() method to obtain the raw output for writing to the FIP
    file.
    """
    def __init__(self, uuid, offset, size, flags):
        self.uuid = uuid
        self.offset = offset
        self.size = size
        self.flags = flags
        self.fip_type = None
        self.data = None
        self.valid = uuid != tools.GetBytes(0, UUID_LEN)
        if self.valid:
            # Look up the friendly name
            matches = {val for (key, val) in FIP_TYPES.items()
                       if val.uuid == uuid}
            if len(matches) == 1:
                self.fip_type = matches.pop().name

    @classmethod
    def from_type(cls, fip_type, data, flags):
        fent = FipEntry(FIP_TYPES[fip_type].uuid, None, len(data), flags)
        fent.fip_type = fip_type
        fent.data = data
        return fent


def decode_fip(data):
    """Decode a FIP into a header and list of FIP entries

    Args:
        data: Data block containing the FMAP

    Returns:
        Tuple:
            header: FipHeader object
            List of FipArea objects
    """
    fields = list(struct.unpack(HEADER_FORMAT, data[:HEADER_LEN]))
    header = FipHeader(*fields)
    fents = []
    pos = HEADER_LEN
    while True:
        fields = list(struct.unpack(ENTRY_FORMAT, data[pos:pos + ENTRY_SIZE]))
        fent = FipEntry(*fields)
        if not fent.valid:
            break
        fent.data = data[fent.offset:fent.offset + fent.size]
        fents.append(fent)
        pos += ENTRY_SIZE
    return header, fents


class FipWriter:
    """Class to handle writing a ARM Trusted Firmware Firmware Image Package

    Usage is something like:

        fip = FipWriter(size)
        fip.add_file('scp-fwu-cfg', tools.ReadFile('something.bin'))
        ...
        data = cbw.get_data()

    Attributes:
    """
    def __init__(self, flags, align):
        self._fip_entries = []
        self._flags = flags
        self._align = align

    def add_file(self, fip_type, data, flags):
        fent = FipEntry.from_type(fip_type, data, flags)
        self._fip_entries.append(fent)

    def _align_to(self, fd, align):
        """Write out pad bytes until a given alignment is reached

        This only aligns if the resulting output would not reach the end of the
        CBFS, since we want to leave the last 4 bytes for the master-header
        pointer.

        Args:
            fd: File objext to write to
            align: Alignment to require (e.g. 4 means pad to next 4-byte
                boundary)
        """
        offset = align_int(fd.tell(), align)
        if offset < self._size:
            self._skip_to(fd, offset)

    def get_data(self):
        """Obtain the full contents of the FIP

        Thhis builds the FIP with headers and all required FIP entries.

        Returns:
            'bytes' type containing the data
        """
        fd = io.BytesIO()
        hdr = struct.pack(HEADER_FORMAT, HEADER_MAGIC, HEADER_SERIAL,
                          self._flags)
        fd.write(hdr)

        # Calculate the position fo the first entry
        offset = len(hdr)
        offset += len(self._fip_entries) * ENTRY_SIZE
        offset += ENTRY_SIZE   # terminating entry

        for fent in self._fip_entries:
            offset = tools.Align(offset, self._align)
            fent.offset = offset
            offset += fent.size

        # Write out the TOC
        for fent in self._fip_entries:
            hdr = struct.pack(ENTRY_FORMAT, fent.uuid, fent.offset, fent.size,
                              fent.flags)
            fd.write(hdr)

        # Write out the entries
        for fent in self._fip_entries:
            fd.seek(fent.offset)
            fd.write(fent.data)

        return fd.getvalue()


def parse_uuids(srcdir):
    """parse_uuids: Parse the firmware_image_package.h file

    Args:
        srcdir: 'arm-trusted-firmware' source directory

    Returns:
        dict:
            key: UUID macro name, e.g. 'UUID_TRUSTED_FWU_CERT'
            value: list:
                file comment comment, e.g. 'ToC Entry UUIDs'
                macro name, e.g. 'UUID_TRUSTED_FWU_CERT'
                uuid as bytes(16)
    """
    re_uuid = re.compile('0x[0-9a-fA-F]{2}')
    re_comment = re.compile(r'^/\* (.*) \*/$')
    fname = os.path.join(srcdir, 'include/tools_share/firmware_image_package.h')
    data = tools.ReadFile(fname, binary=False)
    macros = collections.OrderedDict()
    comment = None
    for linenum, line in enumerate(data.splitlines()):
        if line.startswith('/*'):
            m = re_comment.match(line)
            if m:
                comment = m.group(1)
        else:
            # Example: #define UUID_TOS_FW_CONFIG \
            if 'UUID' in line:
                macro = line.split()[1]
            elif '{{' in line:
                m = re_uuid.findall(line)
                if not m or len(m) != 16:
                    raise ValueError('%s: Cannot parse UUID line %d: Got matches: %s' %
                                     (ttbr_fname, linenum + 1, m))
                uuid = bytes([int(val, 16) for val in m])
                macros[macro] = comment, macro, uuid
    return macros


def parse_names(srcdir):
    """parse_names: Parse the tbbr_config.c file

    Args:
        srcdir: 'arm-trusted-firmware' source directory

    Returns:
        tuple: list of entries
            tuple: entry information
                Description of entry, e.g. 'Non-Trusted Firmware BL33'
                UUID macro, e.g. 'UUID_NON_TRUSTED_FIRMWARE_BL33'
                Name of entry, e.g. 'nt-fw'
    """
    # Extract the .name, .uuid and .cmdline_name values
    re_data = re.compile('\.name = "([^"]*)",\s*\.uuid = (UUID_\w*),\s*\.cmdline_name = "([^"]+)"',
                         re.S)
    fname = os.path.join(srcdir, 'tools/fiptool/tbbr_config.c')
    data = tools.ReadFile(fname, binary=False)

    # Example entry:
    #   {
    #       .name = "Secure Payload BL32 Extra2 (Trusted OS Extra2)",
    #       .uuid = UUID_SECURE_PAYLOAD_BL32_EXTRA2,
    #       .cmdline_name = "tos-fw-extra2"
    #   },
    m = re_data.findall(data)
    if not m:
        raise ValueError('%s: Cannot parse file' % ttbr_fname)
    names = {uuid: (desc, uuid, name) for desc, uuid, name in m}
    return names


def create_code_output(macros, names):
    def _to_hex_list(data):
        """Convert bytes into C code

        Args:
            bytes to convert

        Returns
            String in the format '0x12, 0x34, 0x56...'
        """
        # Use 0x instead of %# since the latter ignores the 0 modifier in
        # Python 3.8.10
        return ', '.join(['0x%02x' % byte for byte in data])

    out = ''
    last_comment = None
    for comment, macro, uuid in macros.values():
        name_entry = names.get(macro)
        if not name_entry:
            print("Warning: UUID '%s' is not mentioned in tbbr_config.c file" %
                  macro)
            continue
        desc, _, name = name_entry
        if last_comment != comment:
            out += '    # %s\n' % comment
            last_comment = comment
        out += """    FipType('%s', '%s',
            [%s,
             %s]),
""" % (name, desc, _to_hex_list(uuid[:8]), _to_hex_list(uuid[8:]))
    return out

def parse_atf_source(srcdir, dstfile):
    # We expect a readme file
    readme_fname = os.path.join(srcdir, 'readme.rst')
    if not os.path.exists(readme_fname):
        raise ValueError("Expected file '%s' - try using -s to specify the arm-trusted-firmware directory" %
                         readme_fname)
    readme = tools.ReadFile(readme_fname, binary=False)
    first_line = 'Trusted Firmware-A'
    if readme.splitlines()[0] != first_line:
        raise ValueError("'%s' does not start with '%s'" %
                         (readme_fname, first_line))
    macros = parse_uuids(srcdir)
    names = parse_names(srcdir)
    output = create_code_output(macros, names)
    orig = tools.ReadFile(OUR_FILE, binary=False)
    re_fip_list = re.compile('(.*FIP_TYPE_LIST = \[).*?(    ] # end.*)', re.S)
    m = re_fip_list.match(orig)
    new_code = m.group(1) + output + m.group(2)
    tools.WriteFile(dstfile, new_code, binary=False)


if __name__ == "__main__":
    epilog = '''fip_util.py: Create table of FIP-entry types'''

    parser = ArgumentParser(epilog=epilog)
    parser.add_argument('-D', '--debug', action='store_true',
        help='Enabling debugging (provides a full traceback on error)')
    parser.add_argument('-o', '--outfile', type=str, default='fip_util.py.out',
        help='Output file to write new fip_util.py file to')
    parser.add_argument('-s', '--src', type=str, default='.',
        help='Directory containing the arm-trusted-firmware source')
    args = parser.parse_args()

    if not args.debug:
        sys.tracebacklimit = 0

    ret_code = parse_atf_source(args.src, args.outfile)
    sys.exit(ret_code)
