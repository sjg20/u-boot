#!/usr/bin/python
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2017 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#

"""Device tree to platform data class

This supports converting device tree data to C structures definitions and
static data.

See doc/driver-model/of-plat.rst for more informaiton
"""

import collections
import copy
import os
import re
import sys

from dtoc import fdt
from dtoc import fdt_util

# When we see these properties we ignore them - i.e. do not create a structure
# member
PROP_IGNORE_LIST = [
    '#address-cells',
    '#gpio-cells',
    '#size-cells',
    'compatible',
    'linux,phandle',
    "status",
    'phandle',
    'u-boot,dm-pre-reloc',
    'u-boot,dm-tpl',
    'u-boot,dm-spl',
]

# C type declarations for the types we support
TYPE_NAMES = {
    fdt.Type.INT: 'fdt32_t',
    fdt.Type.BYTE: 'unsigned char',
    fdt.Type.STRING: 'const char *',
    fdt.Type.BOOL: 'bool',
    fdt.Type.INT64: 'fdt64_t',
}

STRUCT_PREFIX = 'dtd_'
VAL_PREFIX = 'dtv_'

# This holds information about a property which includes phandles.
#
# max_args: integer: Maximum number or arguments that any phandle uses (int).
# args: Number of args for each phandle in the property. The total number of
#     phandles is len(args). This is a list of integers.
PhandleInfo = collections.namedtuple('PhandleInfo', ['max_args', 'args'])

# Holds a single phandle link, allowing a C struct value to be assigned to point
# to a device
#
# var_node: C variable to assign (e.g. 'dtv_mmc.clocks[0].node')
# dev_name: Name of device to assign to (e.g. 'clock')
PhandleLink = collections.namedtuple('PhandleLink', ['var_node', 'dev_name'])


class Driver:
    """Information about a driver in U-Boot

    Attributes:
        compat: Driver data for each compatible string:
            key: Compatible string, e.g. 'rockchip,rk3288-grf'
            value: Driver data, e,g, 'ROCKCHIP_SYSCON_GRF', or None
    """
    def __init__(self, name):
        self.name = name
        self.uclass_id = None
        self.compat = None
        self.priv = ''
        self.platdata = ''
        self.child_platdata = ''
        self.child_priv = ''
        self.used = False

    def __eq__(self, other):
        return (self.name == other.name and
                self.uclass_id == other.uclass_id and
                self.compat == other.compat and
                self.priv == other.priv and
                self.platdata == other.platdata)

    def __repr__(self):
        return ("Driver(name='%s', used=%s, uclass_id='%s', compat=%s, priv=%s)" %
                (self.name, self.used, self.uclass_id, self.compat, self.priv))


class UclassDriver:
    """Holds information about a uclass driver

    Attributes:
        name: Uclass name, e.g. 'i2c' if the driver is for UCLASS_I2C
        uclass_id: Uclass ID, e.g. 'UCLASS_I2C'
        priv: Information about private data, e.g.
            '<i2c.h>, sizeof(struct i2c_priv)'
        per_dev_priv (str): Information about per-device private data
        per_dev_platdata (str): Information about per-device platdata
        devs (list): List of devices in this uclass, each a Node
        node_refs (dict): References in the linked list of devices:
            key (it): Sequence number (0=first, n-1=last, -1=head, n=tail)
            value (str): Reference to the device at that position
    """
    def __init__(self, name):
        self.name = name
        self.uclass_id = None
        self.priv = ''
        self.per_dev_priv = ''
        self.per_dev_platdata = ''
        self.devs = []
        self.node_refs = {}

    def __eq__(self, other):
        return (self.name == other.name and
                self.uclass_id == other.uclass_id and
                self.priv == other.priv)

    def __repr__(self):
        return ("UclassDriver(name='%s', uclass_id='%s')" %
                (self.name, self.uclass_id))


def conv_name_to_c(name):
    """Convert a device-tree name to a C identifier

    This uses multiple replace() calls instead of re.sub() since it is faster
    (400ms for 1m calls versus 1000ms for the 're' version).

    Args:
        name (str): Name to convert
    Return:
        str: String containing the C version of this name
    """
    new = name.replace('@', '_at_')
    new = new.replace('-', '_')
    new = new.replace(',', '_')
    new = new.replace('.', '_')
    return new

def tab_to(num_tabs, line):
    """Append tabs to a line of text to reach a tab stop.

    Args:
        num_tabs (int): Tab stop to obtain (0 = column 0, 1 = column 8, etc.)
        line (str): Line of text to append to

    Returns:
        str: line with the correct number of tabs appeneded. If the line already
        extends past that tab stop then a single space is appended.
    """
    if len(line) >= num_tabs * 8:
        return line + ' '
    return line + '\t' * (num_tabs - len(line) // 8)

def get_value(ftype, value):
    """Get a value as a C expression

    For integers this returns a byte-swapped (little-endian) hex string
    For bytes this returns a hex string, e.g. 0x12
    For strings this returns a literal string enclosed in quotes
    For booleans this return 'true'

    Args:
        ftype (fdt.Type): Data type (fdt_util)
        value (bytes): Data value, as a string of bytes

    Returns:
        str: String representation of the value
    """
    if ftype == fdt.Type.INT:
        return '%#x' % fdt_util.fdt32_to_cpu(value)
    elif ftype == fdt.Type.BYTE:
        char = value[0]
        return '%#x' % (ord(char) if isinstance(char, str) else char)
    elif ftype == fdt.Type.STRING:
        # Handle evil ACPI backslashes by adding another backslash before them.
        # So "\\_SB.GPO0" in the device tree effectively stays like that in C
        return '"%s"' % value.replace('\\', '\\\\')
    elif ftype == fdt.Type.BOOL:
        return 'true'
    else:  # ftype == fdt.Type.INT64:
        return '%#x' % value

def get_compat_name(node):
    """Get the node's list of compatible string as a C identifiers

    Args:
        node (fdt.Node): Node object to check
    Return:
        List of C identifiers for all the compatible strings
    """
    compat = node.props['compatible'].value
    if not isinstance(compat, list):
        compat = [compat]
    return [conv_name_to_c(c) for c in compat]


class DtbPlatdata(object):
    """Provide a means to convert device tree binary data to platform data

    The output of this process is C structures which can be used in space-
    constrained encvironments where the ~3KB code overhead of device tree
    code is not affordable.

    Properties:
        _fdt: Fdt object, referencing the device tree
        _dtb_fname: Filename of the input device tree binary file
        _valid_nodes: A list of Node object with compatible strings. The list
            is ordered by conv_name_to_c(node.name)
        _include_disabled: true to include nodes marked status = "disabled"
        _outfile: The current output file (sys.stdout or a real file)
        _warning_disabled: true to disable warnings about driver names not found
        _lines: Stashed list of output lines for outputting in the future
        _drivers: Dict of valid driver names found in drivers/
            key: Driver name
            value: Driver for that driver
        _driver_aliases: Dict that holds aliases for driver names
            key: Driver alias declared with
                U_BOOT_DRIVER_ALIAS(driver_alias, driver_name)
            value: Driver name declared with U_BOOT_DRIVER(driver_name)
        _drivers_additional: List of additional drivers to use during scanning
        _of_match: Dict holding information about compatible strings
            key: Name of struct udevice_id variable
            value: Dict of compatible info in that variable:
               key: Compatible string, e.g. 'rockchip,rk3288-grf'
               value: Driver data, e,g, 'ROCKCHIP_SYSCON_GRF', or None
        _compat_to_driver: Maps compatible strings to Driver
        _uclass: Dict of uclass information
            key: uclass name (e.g. 'UCLASS_I2C')
            value: UClassDriver
        _instantiate: Instantiate devices so they don't need to be bound at
            run-time
    """
    def __init__(self, dtb_fname, include_disabled, warning_disabled,
                 drivers_additional=None, instantiate=False):
        self._fdt = None
        self._dtb_fname = dtb_fname
        self._valid_nodes = None
        self._include_disabled = include_disabled
        self._outfile = None
        self._warning_disabled = warning_disabled
        self._lines = []
        self._drivers = {}
        self._driver_aliases = {}
        self._drivers_additional = drivers_additional or []
        self._of_match = {}
        self._compat_to_driver = {}
        self._uclass = {}
        self._instantiate = instantiate

    def get_normalized_compat_name(self, node):
        """Get a node's normalized compat name

        Returns a valid driver name by retrieving node's list of compatible
        string as a C identifier and performing a check against _drivers
        and a lookup in driver_aliases printing a warning in case of failure.

        Args:
            node: Node object to check
        Return:
            Tuple:
                Driver name associated with the first compatible string
                List of C identifiers for all the other compatible strings
                    (possibly empty)
                In case of no match found, the return will be the same as
                get_compat_name()
        """
        compat_list_c = get_compat_name(node)

        for compat_c in compat_list_c:
            if not compat_c in self._drivers.keys():
                compat_c = self._driver_aliases.get(compat_c)
                if not compat_c:
                    continue

            aliases_c = compat_list_c
            if compat_c in aliases_c:
                aliases_c.remove(compat_c)
            return compat_c, aliases_c

        if not self._warning_disabled:
            print('WARNING: the driver %s was not found in the driver list'
                  % (compat_list_c[0]))

        return compat_list_c[0], compat_list_c[1:]

    def setup_output(self, fname):
        """Set up the output destination

        Once this is done, future calls to self.out() will output to this
        file.

        Args:
            fname (str): Filename to send output to, or '-' for stdout
        """
        if fname == '-':
            self._outfile = sys.stdout
        else:
            self._outfile = open(fname, 'w')

    def out(self, line):
        """Output a string to the output file

        Args:
            line (str): String to output
        """
        self._outfile.write(line)

    def buf(self, line):
        """Buffer up a string to send later

        Args:
            line (str): String to add to our 'buffer' list
        """
        self._lines.append(line)

    def get_buf(self):
        """Get the contents of the output buffer, and clear it

        Returns:
            list(str): The output buffer, which is then cleared for future use
        """
        lines = self._lines
        self._lines = []
        return lines

    def out_header(self):
        """Output a message indicating that this is an auto-generated file"""
        self.out('''/*
 * DO NOT MODIFY
 *
 * This file was generated by dtoc from a .dtb (device tree binary) file.
 */

''')

    def get_phandle_argc(self, prop, node_name):
        """Check if a node contains phandles

        We have no reliable way of detecting whether a node uses a phandle
        or not. As an interim measure, use a list of known property names.

        Args:
            prop (fdt.Prop): Prop object to check
            node_name (str): Node name, only used for raising an error
        Returns:
            int or None: Number of argument cells is this is a phandle,
                else None
        Raises:
            ValueError: if the phandle cannot be parsed or the required property
                is not present
        """
        if prop.name in ['clocks', 'cd-gpios']:
            if not isinstance(prop.value, list):
                prop.value = [prop.value]
            val = prop.value
            i = 0

            max_args = 0
            args = []
            while i < len(val):
                phandle = fdt_util.fdt32_to_cpu(val[i])
                # If we get to the end of the list, stop. This can happen
                # since some nodes have more phandles in the list than others,
                # but we allocate enough space for the largest list. So those
                # nodes with shorter lists end up with zeroes at the end.
                if not phandle:
                    break
                target = self._fdt.phandle_to_node.get(phandle)
                if not target:
                    raise ValueError("Cannot parse '%s' in node '%s'" %
                                     (prop.name, node_name))
                cells = None
                for prop_name in ['#clock-cells', '#gpio-cells']:
                    cells = target.props.get(prop_name)
                    if cells:
                        break
                if not cells:
                    raise ValueError("Node '%s' has no cells property" %
                                     (target.name))
                num_args = fdt_util.fdt32_to_cpu(cells.value)
                max_args = max(max_args, num_args)
                args.append(num_args)
                i += 1 + num_args
            return PhandleInfo(max_args, args)
        return None

    @staticmethod
    def uclass_id_to_name(uclass_id):
        return uclass_id[len('UCLASS_'):].lower()

    def _parse_uclass_driver(self, buff, fname):
        """Parse a C file to extract uclass driver information contained within

        This parses UCLASS_DRIVER() structs to obtain various pieces of useful
        information.

        It updates the following members:

        Args:
            buff (str): Contents of file
            fname (str): Filename (to use when printing errors)
        """
        uc_drivers = {}

        # Collect the driver name and associated Driver
        driver = None
        re_driver = re.compile(r'UCLASS_DRIVER\((.*)\)')

        # Collect the uclass ID, e.g. 'UCLASS_SPI'
        re_id = re.compile(r'\s*\.id\s*=\s*(UCLASS_[A-Z0-9_]+)')

        # Matches the header/size information for uclass-private data
        re_priv = re.compile('^\s*DM_PRIV\((.*)\)$')

        # Matches the header/size information for per-device uclass data
        re_per_device_priv = re.compile('^\s*DM_PER_DEVICE_PRIV\((.*)\)$')

        # Matches the header/size information for per-device uclass platdata
        re_per_device_plat = re.compile('^\s*DM_PER_DEVICE_PLATDATA\((.*)\)$')

        prefix = ''
        for line in buff.splitlines():
            # Handle line continuation
            if prefix:
                line = prefix + line
                prefix = ''
            if line.endswith('\\'):
                prefix = line[:-1]
                continue

            driver_match = re_driver.search(line)

            # If we have seen UCLASS_DRIVER()...
            if driver:
                m_id = re_id.search(line)
                m_priv = re_priv.match(line)
                m_per_dev_priv = re_per_device_priv.match(line)
                m_per_dev_plat = re_per_device_plat.match(line)
                if m_id:
                    driver.uclass_id = m_id.group(1)
                elif m_priv:
                    driver.priv = m_priv.group(1)
                elif m_per_dev_priv:
                    driver.per_dev_priv = m_per_dev_priv.group(1)
                elif m_per_dev_plat:
                    driver.per_dev_platdata = m_per_dev_plat.group(1)
                elif '};' in line:
                    if not driver.uclass_id:
                        raise ValueError(
                            "%s: Cannot parse uclass ID in driver '%s'" %
                            (fname, driver.name))
                    uc_drivers[driver.uclass_id] = driver
                    driver = None

            elif driver_match:
                driver_name = driver_match.group(1)
                driver = UclassDriver(driver_name)

        self._uclass.update(uc_drivers)

    def _parse_driver(self, buff, fname):
        """Parse a C file to extract driver information contained within

        This parses U_BOOT_DRIVER() structs to obtain various pieces of useful
        information.

        It updates the following members:
            _drivers - updated with new Driver records for each driver found
                in the file
            _of_match - updated with each compatible string found in the file
            _compat_to_driver - Maps compatible string to Driver

        Args:
            buff (str): Contents of file
            fname (str): Filename (to use when printing errors)
        """
        # Dict holding information about compatible strings collected in this
        # function so far
        #    key: Name of struct udevice_id variable
        #    value: Dict of compatible info in that variable:
        #       key: Compatible string, e.g. 'rockchip,rk3288-grf'
        #       value: Driver data, e,g, 'ROCKCHIP_SYSCON_GRF', or None
        of_match = {}

        # Dict holding driver information collected in this function so far
        #    key: Driver name (C name as in U_BOOT_DRIVER(xxx))
        #    value: Driver
        drivers = {}

        # Collect the driver info
        driver = None
        re_driver = re.compile(r'U_BOOT_DRIVER\((.*)\)')

        # Collect the uclass ID, e.g. 'UCLASS_SPI'
        re_id = re.compile(r'\s*\.id\s*=\s*(UCLASS_[A-Z0-9_]+)')

        # Collect the compatible string, e.g. 'rockchip,rk3288-grf'
        compat = None
        re_compat = re.compile('{\s*.compatible\s*=\s*"(.*)"\s*'
                                    '(,\s*.data\s*=\s*(\S*))?\s*},')

        # This is a dict of compatible strings that were found:
        #    key: Compatible string, e.g. 'rockchip,rk3288-grf'
        #    value: Driver data, e,g, 'ROCKCHIP_SYSCON_GRF', or None
        compat_dict = {}

        # Holds the var nane of the udevice_id list, e.g.
        # 'rk3288_syscon_ids_noc' in
        # static const struct udevice_id rk3288_syscon_ids_noc[] = {
        ids_name = None
        re_ids = re.compile('struct udevice_id (.*)\[\]\s*=')

        # Matches the references to the udevice_id list
        re_of_match = re.compile('\.of_match\s*=\s*([a-z0-9_]+),')

        # Matches the header/size information for priv, platdata
        re_priv = re.compile('^\s*DM_PRIV\((.*)\)$')
        re_platdata = re.compile('^\s*DM_PLATDATA\((.*)\)$')
        re_child_platdata = re.compile('^\s*DM_CHILD_PLATDATA\((.*)\)$')
        re_child_priv = re.compile('^\s*DM_CHILD_PRIV\((.*)\)$')

        prefix = ''
        for line in buff.splitlines():
            # Handle line continuation
            if prefix:
                line = prefix + line
                prefix = ''
            if line.endswith('\\'):
                prefix = line[:-1]
                continue

            driver_match = re_driver.search(line)

            # If this line contains U_BOOT_DRIVER()...
            if driver:
                m_id = re_id.search(line)
                m_of_match = re_of_match.search(line)
                m_priv = re_priv.match(line)
                m_platdata = re_platdata.match(line)
                m_cplatdata = re_child_platdata.match(line)
                m_cpriv = re_child_priv.match(line)
                if m_priv:
                    driver.priv = m_priv.group(1)
                elif m_platdata:
                    driver.platdata = m_platdata.group(1)
                elif m_cplatdata:
                    driver.child_platdata = m_cplatdata.group(1)
                elif m_cpriv:
                    driver.child_priv = m_cpriv.group(1)
                elif m_id:
                    driver.uclass_id = m_id.group(1)
                elif m_of_match:
                    compat = m_of_match.group(1)
                elif '};' in line:
                    if driver.uclass_id and compat:
                        if compat not in of_match:
                            raise ValueError(
                                "%s: Unknown compatible var '%s' (found %s)" %
                                (fname, compat, ','.join(of_match.keys())))
                        driver.compat = of_match[compat]
                        drivers[driver.name] = driver

                        # This needs to be deterministic, since a driver may
                        # have multiple compatible strings pointing to it.
                        # We record the one earliest in the alphabet so it
                        # will produce the same result on all machines.
                        for id in of_match[compat]:
                            old = self._compat_to_driver.get(id)
                            if not old or driver.name < old.name:
                                self._compat_to_driver[id] = driver
                    else:
                        # The driver does not have a uclass or compat string.
                        # The first is required but the second is not, so just
                        # ignore this.
                        pass
                    driver = None
                    ids_name = None
                    compat = None
                    compat_dict = {}

            elif ids_name:
                compat_m = re_compat.search(line)
                if compat_m:
                    compat_dict[compat_m.group(1)] = compat_m.group(3)
                elif '};' in line:
                    of_match[ids_name] = compat_dict
                    ids_name = None
            elif driver_match:
                driver_name = driver_match.group(1)
                driver = Driver(driver_name)
            else:
                ids_m = re_ids.search(line)
                if ids_m:
                    ids_name = ids_m.group(1)

        # Make the updates based on what we found
        self._drivers.update(drivers)
        self._of_match.update(of_match)

    def scan_driver(self, fname):
        """Scan a driver file to build a list of driver names and aliases

        It updates the following members:
            _drivers - updated with new Driver records for each driver found
                in the file
            _of_match - updated with each compatible string found in the file
            _compat_to_driver - Maps compatible string to Driver
            _driver_aliases - Maps alias names to driver name

        Args
            fname: Driver filename to scan
        """
        with open(fname, encoding='utf-8') as inf:
            try:
                buff = inf.read()
            except UnicodeDecodeError:
                # This seems to happen on older Python versions
                print("Skipping file '%s' due to unicode error" % fname)
                return

            # If this file has any U_BOOT_DRIVER() declarations, process it to
            # obtain driver information
            if 'U_BOOT_DRIVER' in buff:
                self._parse_driver(buff, fname)
            if 'UCLASS_DRIVER' in buff:
                self._parse_uclass_driver(buff, fname)

            # The following re will search for driver aliases declared as
            # U_BOOT_DRIVER_ALIAS(alias, driver_name)
            driver_aliases = re.findall(
                'U_BOOT_DRIVER_ALIAS\(\s*(\w+)\s*,\s*(\w+)\s*\)',
                buff)

            for alias in driver_aliases: # pragma: no cover
                if len(alias) != 2:
                    continue
                self._driver_aliases[alias[1]] = alias[0]

    def scan_drivers(self, basedir=None):
        """Scan the driver folders to build a list of driver names and aliases

        This procedure will populate self._drivers and self._driver_aliases

        """
        if not basedir:
            basedir = sys.argv[0].replace('tools/dtoc/dtoc', '')
            if basedir == '':
                basedir = './'
        for (dirpath, _, filenames) in os.walk(basedir):
            for fname in filenames:
                if not fname.endswith('.c'):
                    continue
                self.scan_driver(dirpath + '/' + fname)

        for fname in self._drivers_additional:
            if not isinstance(fname, str) or len(fname) == 0:
                continue
            if fname[0] == '/':
                self.scan_driver(fname)
            else:
                self.scan_driver(basedir + '/' + fname)

    def scan_dtb(self):
        """Scan the device tree to obtain a tree of nodes and properties

        Once this is done, self._fdt.GetRoot() can be called to obtain the
        device tree root node, and progress from there.
        """
        self._fdt = fdt.FdtScan(self._dtb_fname)

    def scan_node(self, root, valid_nodes):
        """Scan a node and subnodes to build a tree of node and phandle info

        This adds each node to self._valid_nodes.

        Args:
            root: Root node for scan
            valid_nodes: List of Node objects to add to
        """
        for node in root.subnodes:
            if 'compatible' in node.props:
                status = node.props.get('status')
                if (not self._include_disabled and not status or
                        status.value != 'disabled'):
                    valid_nodes.append(node)

            # recurse to handle any subnodes
            self.scan_node(node, valid_nodes)

    def scan_tree(self):
        """Scan the device tree for useful information

        This fills in the following properties:
            _valid_nodes: A list of nodes we wish to consider include in the
                platform data
        """
        valid_nodes = []
        self.scan_node(self._fdt.GetRoot(), valid_nodes)
        self._valid_nodes = sorted(valid_nodes,
                                   key=lambda x: conv_name_to_c(x.name))
        for idx, node in enumerate(self._valid_nodes):
            node.idx = idx

    @staticmethod
    def get_num_cells(node):
        """Get the number of cells in addresses and sizes for this node

        Args:
            node (fdt.None): Node to check

        Returns:
            Tuple:
                Number of address cells for this node
                Number of size cells for this node
        """
        parent = node.parent
        num_addr, num_size = 2, 2
        if parent:
            addr_prop = parent.props.get('#address-cells')
            size_prop = parent.props.get('#size-cells')
            if addr_prop:
                num_addr = fdt_util.fdt32_to_cpu(addr_prop.value)
            if size_prop:
                num_size = fdt_util.fdt32_to_cpu(size_prop.value)
        return num_addr, num_size

    def scan_reg_sizes(self):
        """Scan for 64-bit 'reg' properties and update the values

        This finds 'reg' properties with 64-bit data and converts the value to
        an array of 64-values. This allows it to be output in a way that the
        C code can read.
        """
        for node in self._valid_nodes:
            reg = node.props.get('reg')
            if not reg:
                continue
            num_addr, num_size = self.get_num_cells(node)
            total = num_addr + num_size

            if reg.type != fdt.Type.INT:
                raise ValueError("Node '%s' reg property is not an int" %
                                 node.name)
            if len(reg.value) % total:
                raise ValueError(
                    "Node '%s' reg property has %d cells "
                    'which is not a multiple of na + ns = %d + %d)' %
                    (node.name, len(reg.value), num_addr, num_size))
            reg.num_addr = num_addr
            reg.num_size = num_size
            if num_addr != 1 or num_size != 1:
                reg.type = fdt.Type.INT64
                i = 0
                new_value = []
                val = reg.value
                if not isinstance(val, list):
                    val = [val]
                while i < len(val):
                    addr = fdt_util.fdt_cells_to_cpu(val[i:], reg.num_addr)
                    i += num_addr
                    size = fdt_util.fdt_cells_to_cpu(val[i:], reg.num_size)
                    i += num_size
                    new_value += [addr, size]
                reg.value = new_value

    def scan_structs(self):
        """Scan the device tree building up the C structures we will use.

        Build a dict keyed by C struct name containing a dict of Prop
        object for each struct field (keyed by property name). Where the
        same struct appears multiple times, try to use the 'widest'
        property, i.e. the one with a type which can express all others.

        Once the widest property is determined, all other properties are
        updated to match that width.

        Returns:
            dict containing structures:
                key (str): Node name, as a C identifier
                value: dict containing structure fields:
                    key (str): Field name
                    value: Prop object with field information
        """
        structs = collections.OrderedDict()
        for node in self._valid_nodes:
            node_name, _ = self.get_normalized_compat_name(node)
            fields = {}

            # Get a list of all the valid properties in this node.
            for name, prop in node.props.items():
                if name not in PROP_IGNORE_LIST and name[0] != '#':
                    fields[name] = copy.deepcopy(prop)

            # If we've seen this node_name before, update the existing struct.
            if node_name in structs:
                struct = structs[node_name]
                for name, prop in fields.items():
                    oldprop = struct.get(name)
                    if oldprop:
                        oldprop.Widen(prop)
                    else:
                        struct[name] = prop

            # Otherwise store this as a new struct.
            else:
                structs[node_name] = fields

        for node in self._valid_nodes:
            node_name, _ = self.get_normalized_compat_name(node)
            struct = structs[node_name]
            for name, prop in node.props.items():
                if name not in PROP_IGNORE_LIST and name[0] != '#':
                    prop.Widen(struct[name])

        return structs

    def scan_phandles(self):
        """Figure out what phandles each node uses

        We need to be careful when outputing nodes that use phandles since
        they must come after the declaration of the phandles in the C file.
        Otherwise we get a compiler error since the phandle struct is not yet
        declared.

        This function adds to each node a list of phandle nodes that the node
        depends on. This allows us to output things in the right order.
        """
        for node in self._valid_nodes:
            node.phandles = set()
            for pname, prop in node.props.items():
                if pname in PROP_IGNORE_LIST or pname[0] == '#':
                    continue
                info = self.get_phandle_argc(prop, node.name)
                if info:
                    # Process the list as pairs of (phandle, id)
                    pos = 0
                    for args in info.args:
                        phandle_cell = prop.value[pos]
                        phandle = fdt_util.fdt32_to_cpu(phandle_cell)
                        target_node = self._fdt.phandle_to_node[phandle]
                        node.phandles.add(target_node)
                        pos += 1 + args


    def generate_structs(self, structs):
        """Generate struct defintions for the platform data

        This writes out the body of a header file consisting of structure
        definitions for node in self._valid_nodes. See the documentation in
        doc/driver-model/of-plat.rst for more information.

        Args:
            structs: dict containing structures:
                key (str): Node name, as a C identifier
                value: dict containing structure fields:
                    key (str): Field name
                    value: Prop object with field information

        """
        self.out_header()
        self.out('#include <stdbool.h>\n')
        self.out('#include <linux/libfdt.h>\n')

        # Output the struct definition
        for name in sorted(structs):
            self.out('struct %s%s {\n' % (STRUCT_PREFIX, name))
            for pname in sorted(structs[name]):
                prop = structs[name][pname]
                info = self.get_phandle_argc(prop, structs[name])
                if info:
                    # For phandles, include a reference to the target
                    struct_name = 'struct phandle_%d_arg' % info.max_args
                    self.out('\t%s%s[%d]' % (tab_to(2, struct_name),
                                             conv_name_to_c(prop.name),
                                             len(info.args)))
                else:
                    ptype = TYPE_NAMES[prop.type]
                    self.out('\t%s%s' % (tab_to(2, ptype),
                                         conv_name_to_c(prop.name)))
                    if isinstance(prop.value, list):
                        self.out('[%d]' % len(prop.value))
                self.out(';\n')
            self.out('};\n')

    def _output_list(self, node, prop):
        """Output the C code for a devicetree property that holds a list

        Args:
            node (fdt.Node): Node to output
            prop (fdt.Prop): Prop to output
        """
        self.buf('{')
        vals = []
        # For phandles, output a reference to the platform data
        # of the target node.
        info = self.get_phandle_argc(prop, node.name)
        if info:
            # Process the list as pairs of (phandle, id)
            pos = 0
            for args in info.args:
                phandle_cell = prop.value[pos]
                phandle = fdt_util.fdt32_to_cpu(phandle_cell)
                target_node = self._fdt.phandle_to_node[phandle]
                arg_values = []
                for i in range(args):
                    arg_values.append(
                        str(fdt_util.fdt32_to_cpu(prop.value[pos + 1 + i])))
                pos += 1 + args
                vals.append('\t{%d, {%s}}' % (target_node.idx,
                                                ', '.join(arg_values)))
            for val in vals:
                self.buf('\n\t\t%s,' % val)
        else:
            for val in prop.value:
                vals.append(get_value(prop.type, val))

            # Put 8 values per line to avoid very long lines.
            for i in range(0, len(vals), 8):
                if i:
                    self.buf(',\n\t\t')
                self.buf(', '.join(vals[i:i + 8]))
        self.buf('}')

    def _declare_device(self, var_name, struct_name, node_parent):
        """Add a device declaration to the output

        This declares a U_BOOT_DEVICE() for the device being processed

        Args:
            var_name: C name for the node
            struct_name: Name for the dt struct associated with the node
            node_parent: Parent of the node (or None if none)
        """
        self.buf('U_BOOT_DEVICE(%s) = {\n' % var_name)
        self.buf('\t.name\t\t= "%s",\n' % struct_name)
        self.buf('\t.platdata\t= &%s%s,\n' % (VAL_PREFIX, var_name))
        self.buf('\t.platdata_size\t= sizeof(%s%s),\n' % (VAL_PREFIX, var_name))
        idx = -1
        if node_parent and node_parent in self._valid_nodes:
            idx = node_parent.idx
        self.buf('\t.parent_idx\t= %d,\n' % idx)
        self.buf('};\n')
        self.buf('\n')

    def prep_priv(self, info, name, suffix):
        if not info:
            return None
        parts = info.split(',')
        if len(parts) == 2:
            hdr, struc = parts
        else:
            hdr = None
            struc = parts[0]
        var_name = '_%s%s' % (name, suffix)
        if hdr:
            self.buf('#include %s\n' % hdr)
        section = '__attribute__ ((section (".data")))'
        return var_name, struc, section

    def alloc_priv(self, info, name, suffix='_priv'):
        result = self.prep_priv(info, name, suffix)
        if not result:
            return None
        var_name, struc, section = result
        self.buf('u8 %s[sizeof(%s)]\n\t%s;\n' % (var_name, struc.strip(),
                                                 section))
        return var_name

    def alloc_plat(self, info, name, dt_platdata, node):
        result = self.prep_priv(info, name, '_plat')
        if not result:
            return None
        var_name, struc, section = result
        self.buf('%s %s = {\n' % (struc.strip(), var_name))
        self.buf('\t.dtplat = {\n')
        for pname in sorted(node.props):
            self._output_prop(node, node.props[pname], 2)
        self.buf('\t},\n')
        self.buf('} %s;\n' % section)
        return '&' + var_name

    def _declare_device_inst(self, driver, var_name, struct_name,
			     parent_driver, node, uclass):
        """Add a device instance declaration to the output

        This declares a U_BOOT_DEVICE_INST() for the device being processed

        Args:
            var_name: C name for the node
            struct_name: Name for the dt struct associated with the node
            parent_driver: Driver for the node's parent, or None if none
        """
        self.buf('DM_DECL_DRIVER(%s);\n' % struct_name);
        self.buf('\n')
        plat_name = self.alloc_plat(driver.platdata, driver.name, var_name,
                                    node)
        priv_name = self.alloc_priv(driver.priv, driver.name)
        parent_plat_name = None
        parent_priv_name = None
        if parent_driver:
            parent_plat_name = self.alloc_priv(parent_driver.child_platdata,
                                               driver.name, '_parent_plat')
            parent_priv_name = self.alloc_priv(parent_driver.child_priv,
                                               driver.name, '_parent_priv')
        uclass_plat_name = self.alloc_priv(uclass.per_dev_platdata, driver.name)
        uclass_priv_name = self.alloc_priv(uclass.per_dev_priv, driver.name)

        self.buf('U_BOOT_DEVICE_INST(%s) = {\n' % var_name)
        self.buf('\t.driver\t\t= DM_REF_DRIVER(%s),\n' % struct_name)
        self.buf('\t.name\t\t= "%s",\n' % struct_name)
        if plat_name:
            self.buf('\t.platdata\t= %s,\n' % plat_name)
        else:
            self.buf('\t.platdata\t= &%s%s,\n' % (VAL_PREFIX, var_name))
        if parent_plat_name:
            self.buf('\t.parent_platdata = %s,\n' % parent_plat_name)
        if uclass_plat_name:
            self.buf('\t.uclass_platdata = %s,\n' % uclass_plat_name)
        driver_date = None

        compat_list = node.props['compatible'].value
        if not isinstance(compat_list, list):
            compat_list = [compat_list]
        for compat in compat_list:
            driver_data = driver.compat.get(compat)
            if driver_data:
                self.buf('\t.driver_data\t= %s,\n' % driver_data)
                break
        if node.parent and node.parent.parent:
            self.buf('\t.parent\t\t= U_BOOT_DEVICE_REF(%s),\n' %
                     conv_name_to_c(node.parent.name))
        if priv_name:
            self.buf('\t.priv\t\t= %s,\n' % priv_name)
        self.buf('\t.uclass\t= DM_REF_UCLASS_INST(%s),\n' % uclass.name)

        if uclass_priv_name:
            self.buf('\t.uclass_priv = %s,\n' % uclass_priv_name)
        if parent_priv_name:
            self.buf('\t.parent_priv\t= %s,\n' % parent_priv_name)
        self.list_node('uclass_node', uclass.node_refs, node.uclass_seq)
        self.buf('};\n')
        self.buf('\n')
        return parent_plat_name

    def _output_prop(self, node, prop, tabs=1):
        """Output a line containing the value of a struct member

        Args:
            node: Node being output
            prop: Prop object to output
        """
        if prop.name in PROP_IGNORE_LIST or prop.name[0] == '#':
            return
        member_name = conv_name_to_c(prop.name)
        self.buf('%s%s= ' % ('\t' * tabs, tab_to(3, '.' + member_name)))

        # Special handling for lists
        if isinstance(prop.value, list):
            self._output_list(node, prop)
        else:
            self.buf(get_value(prop.type, prop.value))
        self.buf(',\n')

    def _output_values(self, var_name, struct_name, node):
        """Output the definition of a device's struct values

        Args:
            var_name: C name for the node
            struct_name: Name for the dt struct associated with the node
            node: Node being output
        """
        self.buf('static struct %s%s %s%s = {\n' %
                 (STRUCT_PREFIX, struct_name, VAL_PREFIX, var_name))
        for pname in sorted(node.props):
            self._output_prop(node, node.props[pname])
        self.buf('};\n')

    def output_node(self, node):
        """Output the C code for a node

        Args:
            node (fdt.Node): node to output
        """
        struct_name, _ = self.get_normalized_compat_name(node)
        var_name = conv_name_to_c(node.name)

        driver = node.driver
        parent_driver = node.parent_driver

        self.buf('/*\n')
        self.buf(' * Node %s index %d\n' % (node.path, node.idx))
        self.buf(' * driver %s parent %s\n' % (driver.name,
                 parent_driver.name if parent_driver else 'None'))
        self.buf('*/\n')

        if not self._instantiate or not driver.platdata:
            self._output_values(var_name, struct_name, node)
        if self._instantiate:
            self._declare_device_inst(driver, var_name, struct_name,
                                      parent_driver, node, node.uclass)
        else:
            self._declare_device(var_name, struct_name, node.parent)

        self.out(''.join(self.get_buf()))

    def list_node(self, member, node_refs, seq):
        self.buf('\t.%s\t= {\n' % member)
        self.buf('\t\t.prev = %s,\n' % node_refs[seq - 1])
        self.buf('\t\t.next = %s,\n' % node_refs[seq + 1])
        self.buf('\t},\n')

    def _output_uclasses(self):
        uclass_list = set()
        for driver in self._drivers.values():
            if driver.used:
                uclass_list.add(driver.uclass_id)
        self.buf('/* uclass declarations */\n')
        uclass_list = sorted(list(uclass_list))
        prev_uc = 'NULL /* Set up at runtime */'

        uclass_node = {}
        for seq, uclass_id in enumerate(uclass_list):
            uc_name = self.uclass_id_to_name(uclass_id)
            self.buf('DM_DECL_UCLASS_DRIVER(%s);\n' % uc_name)
            self.buf('DM_DECL_UCLASS_INST(%s);\n' % uc_name)
            uclass_node[seq] = ('&DM_REF_UCLASS_INST(%s)->sibling_node' %
                                      uc_name)
        uclass_node[-1] = 'NULL /* Set up at runtime */'
        uclass_node[len(uclass_list)] = 'NULL /* Set up at runtime */'
        self.buf('\n')

        for seq, uclass_id in enumerate(uclass_list):
            uc_drv = self._uclass.get(uclass_id)
            if not uc_drv:
                raise ValueError('Cannot find uclass driver for %s (have %s)'
                                 % (uc_drv, ', '.join(self._uclass.keys())))
            ref = '&DM_REF_UCLASS_INST(%s)->dev_head' % uc_name
            uc_drv.node_refs[-1] = ref
            uc_drv.node_refs[len(uc_drv.devs)] = ref

            priv_name = self.alloc_priv(uc_drv.priv, uc_drv.name)

            uc_name = self.uclass_id_to_name(uclass_id)
            self.buf('UCLASS_INST(%s) = {\n' % uc_name)
            if priv_name:
                self.buf('\t.priv\t\t= %s,\n' % priv_name)
            self.buf('\t.uc_drv\t\t= DM_REF_UCLASS_DRIVER(%s),\n' % uc_name)
            self.list_node('sibling_node', uclass_node, seq)
            if uc_drv.devs:
                self.buf('\t.dev_head\t= {\n')
                last = uc_drv.devs[-1]
                first = uc_drv.devs[0]
                self.buf('\t\t.prev = &%s->uclass_node,\n' % last.dev_ref)
                self.buf('\t\t.next = &%s->uclass_node,\n' % first.dev_ref)
                self.buf('\t},\n')
            self.buf('};\n')
            self.buf('\n')

    def generate_tables(self):
        """Generate device defintions for the platform data

        This writes out C platform data initialisation data and
        U_BOOT_DEVICE() declarations for each valid node. Where a node has
        multiple compatible strings, a #define is used to make them equivalent.

        See the documentation in doc/driver-model/of-plat.rst for more
        information.
        """
        self.out_header()
        self.out('/* Allow use of U_BOOT_DEVICE() in this file */\n')
        self.out('#define DT_PLATDATA_C\n')
        self.out('\n')
        self.out('#include <common.h>\n')
        self.out('#include <dm.h>\n')
        self.out('#include <dt-structs.h>\n')
        self.out('\n')
        nodes_to_output = list(self._valid_nodes)

        # Figure out which drivers we actually use
        for node in nodes_to_output:
            struct_name, _ = self.get_normalized_compat_name(node)
            driver = self._drivers.get(struct_name)
            if driver:
                driver.used = True

        for node in nodes_to_output:
            self.buf('U_BOOT_DEVICE_DECL(%s);\n' % conv_name_to_c(node.name))
            node.dev_ref = 'U_BOOT_DEVICE_REF(%s)' % conv_name_to_c(node.name)
            struct_name, _ = self.get_normalized_compat_name(node)
            driver = self._drivers.get(struct_name)
            if not driver:
                raise ValueError("Cannot parse/find driver for '%s'" %
                                 struct_name)
            node.driver = driver
            uclass = self._uclass.get(driver.uclass_id)
            if not uclass:
                raise ValueError("Cannot parse/find uclass '%s' for driver '%s'" %
                                (driver.uclass_id, struct_name))
            node.uclass = uclass
            node.uclass_seq = len(node.uclass.devs)
            node.uclass.devs.append(node)
            uclass.node_refs[node.uclass_seq] = '&%s->uclass_node' % node.dev_ref
            parent_driver = None
            parent_struct_name = None
            if node.parent in self._valid_nodes:
                parent_struct_name, _ = self.get_normalized_compat_name(
                    node.parent)
                parent_driver = self._drivers.get(parent_struct_name)
                if not parent_driver:
                    raise ValueError("Cannot parse/find driver for '%s'" %
                                    parent_struct_name)
            node.parent_driver = parent_driver
        self.buf('\n')

        if self._instantiate:
            self._output_uclasses()

        # Keep outputing nodes until there is none left
        while nodes_to_output:
            node = nodes_to_output[0]
            # Output all the node's dependencies first
            for req_node in node.phandles:
                if req_node in nodes_to_output:
                    self.output_node(req_node)
                    nodes_to_output.remove(req_node)
            self.output_node(node)
            nodes_to_output.remove(node)

        # Define dm_populate_phandle_data() which will add the linking between
        # nodes using DM_GET_DEVICE
        # dtv_dmc_at_xxx.clocks[0].node = DM_GET_DEVICE(clock_controller_at_xxx)
        self.buf('void dm_populate_phandle_data(void) {\n')
        self.buf('}\n')

        self.out(''.join(self.get_buf()))

def run_steps(args, dtb_file, include_disabled, output, warning_disabled=False,
              drivers_additional=None, instantiate=False, basedir=None):
    """Run all the steps of the dtoc tool

    Args:
        args (list): List of non-option arguments provided to the problem
        dtb_file (str): Filename of dtb file to process
        include_disabled (bool): True to include disabled nodes
        output (str): Name of output file
        warning_disabled (bool): True to avoid showing warnings about missing
            drivers
       _drivers_additional (list): List of additional drivers to use during
            scanning
        instantiate: Instantiate devices so they don't need to be bound at
            run-time
    Raises:
        ValueError: if args has no command, or an unknown command
    """
    if not args:
        raise ValueError('Please specify a command: struct, platdata')

    plat = DtbPlatdata(dtb_file, include_disabled, warning_disabled,
                       drivers_additional, instantiate)
    plat.scan_drivers(basedir)
    plat.scan_dtb()
    plat.scan_tree()
    plat.scan_reg_sizes()
    plat.setup_output(output)
    structs = plat.scan_structs()
    plat.scan_phandles()

    for cmd in args[0].split(','):
        if cmd == 'struct':
            plat.generate_structs(structs)
        elif cmd == 'platdata':
            plat.generate_tables()
        else:
            raise ValueError("Unknown command '%s': (use: struct, platdata)" %
                             cmd)
