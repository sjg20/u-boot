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
import sys
import os
import re

from dtoc import fdt
from dtoc import fdt_util
from patman import tools

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

class DriverInfo:
    def __init__(self, name, uclass_id, compat):
        self.name = name
        self.uclass_id = uclass_id
        self.compat = compat
        self.priv_size = 0

    def __eq__(self, other):
        return (self.name == other.name and
                self.uclass_id == other.uclass_id and
                self.compat == other.compat and
                self.priv_size == other.priv_size)

    def __repr__(self):
        return ("DriverInfo(name='%s', uclass_id='%s', compat=%s, priv_size=%s)" %
                (self.name, self.uclass_id, self.compat, self.priv_size))

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
        ch = value[0]
        return '%#x' % (ord(ch) if isinstance(ch, str) else ch)
    elif ftype == fdt.Type.STRING:
        return '"%s"' % value
    elif ftype == fdt.Type.BOOL:
        return 'true'
    else:  # ftype == fdt.Type.INT64:
        return '%#x' % value

def get_compat_name(node):
    """Get a node's first compatible string as a C identifier

    Args:
        node (fdt.Node): Node object to check
    Return:
        Tuple:
            C identifier for the first compatible string
            List of C identifiers for all the other compatible strings
                (possibly empty)
    """
    compat = node.props['compatible'].value
    aliases = []
    if isinstance(compat, list):
        compat, aliases = compat[0], compat[1:]
    return conv_name_to_c(compat), [conv_name_to_c(a) for a in aliases]


class DtbPlatdata(object):
    """Provide a means to convert device tree binary data to platform data

    The output of this process is C structures which can be used in space-
    constrained encvironments where the ~3KB code overhead of device tree
    code is not affordable.

    Properties:
        _fdt: Fdt object, referencing the device tree
        _dtb_fname: Filename of the input device tree binary file
        _config_fname: Filename of the .config file for the build
        _valid_nodes: A list of Node object with compatible strings
        _include_disabled: true to include nodes marked status = "disabled"
        _outfile: The current output file (sys.stdout or a real file)
        _lines: Stashed list of output lines for outputting in the future
        _aliases: Dict of aliases read from the DT:
            key: alias name
            value: node path
        _tiny_uclasses: List of uclass names that are marked as 'tiny'
    """
    def __init__(self, dtb_fname, config_fname, include_disabled):
        self._fdt = None
        self._dtb_fname = dtb_fname
        self._config_fname = config_fname
        self._valid_nodes = None
        self._include_disabled = include_disabled
        self._outfile = None
        self._lines = []
        self._compat_aliases = {}
        self._drivers = {}
        self._driver_aliases = {}
        self._links = []
        self._aliases = {}
        self._aliases_by_path = {}
        self._tiny_uclasses = []
        self._of_match = {}
        self._compat_to_driver = {}

    def get_normalized_compat_name(self, node):
        compat_c, aliases_c = get_compat_name(node)
        if compat_c not in self._drivers:
            try: # pragma: no cover
                compat_c_old = compat_c
                compat_c = self._driver_aliases[compat_c]
                aliases_c = [compat_c_old] + aliases
            except:
                pass
                #print('WARNING: the driver %s was not found in the driver list' % (compat_c))

        return compat_c, aliases_c

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

    def close_output(self):
        if self._outfile is not sys.stdout:
            self._outfile.close()

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

    def scan_driver(self, fn):
        f = open(fn)

        b = f.read()

        drivers = {}
        m_drivers = re.findall(r'U_BOOT_DRIVER\((.*)\)', b)
        if m_drivers:
            driver_name = None
            uclass_id = None
            ids_name = None
            compat_list = []
            compat = None
            re_id = re.compile(r'\s*\.id\s*=\s*(UCLASS_[A-Z0-9_]+)')
            re_ids = re.compile('struct udevice_id (.*)\[\]\s*=')
            re_compat = re.compile('{\s*.compatible\s*=\s*"(.*)"\s*},')
            re_of_match = re.compile('\.of_match\s*=\s*([a-z0-9_]+),')
            re_tiny_priv = re.compile('^\s*DM_TINY_PRIV\((.*)\)$')
            tiny_name = None

            for line in b.splitlines():
                if driver_name:
                    id_m = re_id.search(line)
                    id_of_match = re_of_match.search(line)
                    if id_m:
                        uclass_id = id_m.group(1)
                    elif id_of_match:
                        compat = id_of_match.group(1)
                    elif '};' in line:
                        if uclass_id and compat:
                            if compat not in self._of_match:
                                raise ValueError("%s: Unknown compatible var '%s' (found %s)" %
                                                 (fn, compat, ','.join(self._of_match.keys())))
                            drivers[driver_name] = DriverInfo(
                                driver_name, uclass_id, self._of_match[compat])
                            for val in self._of_match[compat]:
                                self._compat_to_driver[val] = drivers[driver_name]
                        else:
                            pass
                            #print("%s: Cannot find .id/.of_match in driver '%s': uclass_id=%s, compat=%s" %
                                  #(fn, driver_name, uclass_id, compat))
                        driver_name = None
                        uclass_id = None
                        ids_name = None
                        compat = None
                        compat_list = []
                elif tiny_name:
                    tiny_priv = re_tiny_priv.match(line)
                    if tiny_priv:
                        drivers[tiny_name].priv_size = tiny_priv.group(1)
                    elif '};' in line:
                        tiny_name = None
                elif ids_name:
                    compat_m = re_compat.search(line)
                    if compat_m:
                        compat_list.append(compat_m.group(1))
                    elif '};' in line:
                        self._of_match[ids_name] = compat_list
                        ids_name = None
                elif 'U_BOOT_DRIVER' in line:
                    match = re.search(r'U_BOOT_DRIVER\((.*)\)', line)
                    if match:
                        driver_name = match.group(1)
                elif 'U_BOOT_TINY_DRIVER' in line:
                    match = re.search(r'U_BOOT_TINY_DRIVER\((.*)\)', line)
                    if match:
                        tiny_name = match.group(1)
                        if tiny_name not in drivers:
                            raise ValueError("%s: Tiny driver '%s' must have a corresponding full driver in the same file (found %s)" %
                                             (fn, tiny_name, drivers))
                else:
                    ids_m = re_ids.search(line)
                    if ids_m:
                        ids_name = ids_m.group(1)

        #for d in m_drivers:
            #self._drivers.append(d)
        self._drivers.update(drivers)

        driver_aliases = re.findall(
            r'U_BOOT_DRIVER_ALIAS\(\s*(\w+)\s*,\s*(\w+)\s*\)', b)

        for a in driver_aliases: # pragma: no cover
            try:
                self._driver_aliases[a[1]] = a[0]
            except:
                pass

    def scan_drivers(self):
        """Scan the driver folders to build a list of driver names and possible
        aliases
        """
        for (dirpath, dirnames, filenames) in os.walk('/home/sglass/u'):
            for fn in filenames:
                if not fn.endswith('.c'):
                    continue
                self.scan_driver(dirpath + '/' + fn)

    def scan_dtb(self):
        """Scan the device tree to obtain a tree of nodes and properties

        Once this is done, self._fdt.GetRoot() can be called to obtain the
        device tree root node, and progress from there.
        """
        self._fdt = fdt.FdtScan(self._dtb_fname)

    def scan_node(self, parent, level):
        """Scan a node and subnodes to build a tree of node and phandle info

        This adds each node to self._valid_nodes.

        Args:
            parent: Parent node for scan
        """
        for node in parent.subnodes:
            if 'compatible' in node.props:
                status = node.props.get('status')
                if (not self._include_disabled and not status or
                        status.value != 'disabled'):
                    self._valid_nodes.append(node)

            if level == 0 and node.name == 'aliases':
                for prop in node.props.values():
                    self._aliases[prop.name] = prop.value
                    match = re.match('^(.*[a-z])[0-9]+', prop.name)
                    if match:
                        self._aliases_by_path[prop.value] = match.group(1)

            # recurse to handle any subnodes
            self.scan_node(node, level + 1)

    def scan_tree(self):
        """Scan the device tree for useful information

        This fills in the following properties:
            _valid_nodes: A list of nodes we wish to consider include in the
                platform data
        """
        self._valid_nodes = []
        self.scan_node(self._fdt.GetRoot(), 0)

    def parse_config(self, config_data):
        tiny_list = re.findall(r'CONFIG_[ST]PL_TINY_(.*)=y', config_data)
        self._tiny_uclasses = [n.lower() for n in tiny_list]

    def scan_config(self):
        if self._config_fname:
            self.parse_config(tools.ReadFile(self._config_fname, binary=False))
        for node in self._valid_nodes:
            node.is_tiny = False
            alias = self._aliases_by_path.get(node.path)
            if alias and alias in self._tiny_uclasses:
                node.is_tiny = True

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
        na, ns = 2, 2
        if parent:
            na_prop = parent.props.get('#address-cells')
            ns_prop = parent.props.get('#size-cells')
            if na_prop:
                na = fdt_util.fdt32_to_cpu(na_prop.value)
            if ns_prop:
                ns = fdt_util.fdt32_to_cpu(ns_prop.value)
        return na, ns

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
            na, ns = self.get_num_cells(node)
            total = na + ns

            if reg.type != fdt.Type.INT:
                raise ValueError("Node '%s' reg property is not an int" %
                                 node.name)
            if len(reg.value) % total:
                raise ValueError(
                    "Node '%s' reg property has %d cells "
                    'which is not a multiple of na + ns = %d + %d)' %
                    (node.name, len(reg.value), na, ns))
            reg.na = na
            reg.ns = ns
            if na != 1 or ns != 1:
                reg.type = fdt.Type.INT64
                i = 0
                new_value = []
                val = reg.value
                if not isinstance(val, list):
                    val = [val]
                while i < len(val):
                    addr = fdt_util.fdt_cells_to_cpu(val[i:], reg.na)
                    i += na
                    size = fdt_util.fdt_cells_to_cpu(val[i:], reg.ns)
                    i += ns
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
        """
        structs = {}
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

        upto = 0
        for node in self._valid_nodes:
            node_name, _ = self.get_normalized_compat_name(node)
            struct = structs[node_name]
            for name, prop in node.props.items():
                if name not in PROP_IGNORE_LIST and name[0] != '#':
                    prop.Widen(struct[name])
            upto += 1

            struct_name, aliases = self.get_normalized_compat_name(node)
            for alias in aliases:
                self._compat_aliases[alias] = struct_name

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

        for alias, struct_name in self._compat_aliases.items():
            if alias not in sorted(structs):
                self.out('#define %s%s %s%s\n'% (STRUCT_PREFIX, alias,
                                                 STRUCT_PREFIX, struct_name))

    def output_node(self, node):
        """Output the C code for a node

        Args:
            node (fdt.Node): node to output
        """
        def _output_list(node, prop):
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
                item = 0
                for args in info.args:
                    phandle_cell = prop.value[pos]
                    phandle = fdt_util.fdt32_to_cpu(phandle_cell)
                    target_node = self._fdt.phandle_to_node[phandle]
                    name = conv_name_to_c(target_node.name)
                    arg_values = []
                    for i in range(args):
                        arg_values.append(str(fdt_util.fdt32_to_cpu(
                            prop.value[pos + 1 + i])))
                    pos += 1 + args
                    vals.append('\t{NULL, {%s}}' % (', '.join(arg_values)))
                    phandle_entry = ('%s%s.%s[%d].node = DM_GET_DEVICE(%s)' %
                        (VAL_PREFIX, var_name, member_name, item, name))
                    self._links.append(phandle_entry)
                    item += 1
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

        struct_name, _ = self.get_normalized_compat_name(node)
        var_name = conv_name_to_c(node.name)

        # Tiny devices don't have 'static' since it is used by the driver
        self.buf('static struct %s%s %s%s = {\n' %
                 (STRUCT_PREFIX, struct_name, VAL_PREFIX, var_name))
        for pname in sorted(node.props):
            prop = node.props[pname]
            if pname in PROP_IGNORE_LIST or pname[0] == '#':
                continue
            member_name = conv_name_to_c(prop.name)
            self.buf('\t%s= ' % tab_to(3, '.' + member_name))

            # Special handling for lists
            if isinstance(prop.value, list):
                _output_list(node, prop)
            else:
                self.buf(get_value(prop.type, prop.value))
            self.buf(',\n')
        self.buf('};\n')

        if node.is_tiny:
            val = node.props['compatible'].value
            if not isinstance(val, list):
                val = [val]
            for compat in val:
                driver = self._compat_to_driver.get(compat)
                if driver:
                    break
            if not driver:
                raise ValueError("Cant' find driver for compatible '%s' (%s)'" %
                                 (', '.val))
            self.buf('DM_DECL_TINY_DRIVER(%s);\n' % driver.name);
            priv_name = None
            inline = True
            if inline and driver.priv_size:
                parts = driver.priv_size.split(',')
                if len(parts) == 2:
                    hdr, size = parts
                else:
                    hdr = None
                    size = parts[0]
                priv_name = '_%s_priv' % var_name
                if hdr:
                    self.buf('#include %s\n' % hdr)
                section = '__attribute__ ((section (".data")))'

                self.buf('u8 %s[%s] %s;\n' % (priv_name, size.strip(), section))

            self.buf('U_BOOT_TINY_DEVICE(%s) = {\n' % var_name)
            self.buf('\t.dtplat\t\t= &%s%s,\n' % (VAL_PREFIX, var_name))
            self.buf('\t.drv\t\t= DM_REF_TINY_DRIVER(%s),\n' % driver.name)
            if priv_name:
                self.buf('\t.priv\t\t= %s,\n' % priv_name)
            self.buf('};\n')
            self.buf('\n')
        else:
            # Add a device declaration
            self.buf('U_BOOT_DEVICE(%s) = {\n' % var_name)
            self.buf('\t.name\t\t= "%s",\n' % struct_name)
            self.buf('\t.platdata\t= &%s%s,\n' % (VAL_PREFIX, var_name))
            self.buf('\t.platdata_size\t= sizeof(%s%s),\n' %
                     (VAL_PREFIX, var_name))
            self.buf('\t.dev\t\t= NULL,\n')
            self.buf('};\n')
            self.buf('\n')

        self.out(''.join(self.get_buf()))

    def generate_tables(self):
        """Generate device defintions for the platform data

        This writes out C platform data initialisation data and
        U_BOOT_DEVICE() declarations for each valid node. Where a node has
        multiple compatible strings, a #define is used to make them equivalent.

        See the documentation in doc/driver-model/of-plat.rst for more
        information.
        """
        self.out_header()
        self.out('#include <common.h>\n')
        self.out('#include <dm.h>\n')
        self.out('#include <dt-structs.h>\n')
        self.out('\n')

        self.out('/*\n')
        self.out(' * Tiny uclasses: %s\n' % (', '.join(self._tiny_uclasses)))
        self.out(' * Aliases with CONFIG_SPL_TINY_... enabled\n')
        for path, alias in self._aliases_by_path.items():
            if alias in self._tiny_uclasses:
                self.out(' * %s: %s\n' % (path, alias))
        self.out('*/\n')
        self.out('\n')
        nodes_to_output = list(self._valid_nodes)

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

        self.buf('void populate_phandle_data(void) {\n')
        for l in self._links:
            self.buf(('\t%s;\n' % l))
        self.buf('}\n')

        self.out(''.join(self.get_buf()))
        self.close_output()

def run_steps(args, dtb_file, config_file, include_disabled, output):
    """Run all the steps of the dtoc tool

    Args:
        args (list): List of non-option arguments provided to the problem
        dtb_file (str): Filename of dtb file to process
        include_disabled (bool): True to include disabled nodes
        output (str): Name of output file
    Raises:
        ValueError: if args has no command, or an unknown command
    """
    if not args:
        raise ValueError('Please specify a command: struct, platdata')

    plat = DtbPlatdata(dtb_file, config_file, include_disabled)
    plat.scan_drivers()
    plat.scan_dtb()
    plat.scan_tree()
    plat.scan_config()
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
    return plat
