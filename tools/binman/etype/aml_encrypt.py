# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for producing an image using aml-encrypt-g12a
#

from collections import OrderedDict

from binman.entry import Entry
from binman.etype.section import Entry_section
from binman.etype.blob_ext import Entry_blob_ext
from binman.etype.blob_ext_list import Entry_blob_ext_list
from dtoc import fdt_util
from patman import tools
from patman import tout

DDR_FW_COUNT = 9

class Entry_aml_encrypt(Entry_section):
    """Amlogic encryption support

    Some Amlogic chips use encryption with various firmware binaries. This
    entry supports running one of the tools to generate the required data
    formats.

    Available parameters are:

    aml-algo
        Algorithm to use, either "g12a" or "g12b"

    aml-op
        Operation to perform, one of "bootmk", "bl2sig", "bl3sig", "bl30sig"

    aml-level
        Level, typically "v3"

    aml-compress
        Optional compression type, e.g. "lz4"

    aml-type
        Binary type to process, one of "bl30", "bl31", "bl32"

    The data to pass to the tool for processing is defined by an 'aml-input'
    property (for a single file) or subnode (for more flexibility). The subnode
    can be any entry type, including a section.

    For cases where multiple inputs are provided to one invocation of the tool,
    these are named, again using subnodes, corresponding to the tool flags.
    Available inputs are acs, bl2, bl30, sbl301, bl31, bl33 and aml-ddrfw. The
    last one is a bit different, in that it takes the form of a blob-ext-list,
    i.e. it has a 'filenames' property with a list of the DDR firmware binaries.

    Here is an example::

        /* run --bootmk on all the included inputs */
        aml-encrypt {
            missing-msg = "aml-encrypt";
            aml-algo = "g12a";
            aml-op = "bootmk";
            aml-level = "v3";

            /* produce a bl2, containing signed bl2 binaries */
            bl2 {
                type = "aml-encrypt";
                aml-algo = "g12a";
                aml-op = "bl2sig";

                /* sign the binary contaiing bl2 and acs */
                aml-input {
                    type = "section";
                    bl2 {
                        type = "blob-ext";
                        size = <0xe000>;
                        filename = "bl2.bin";
                    };
                    acs {
                        type = "blob-ext";
                        size = <0x1000>;
                        filename = "acs.bin";
                    };
                };
            };

            /* produce a bl30, containing signed bl30 binaries */
            bl30 {
                type = "aml-encrypt";
                aml-algo = "g12a";
                aml-op = "bl3sig";
                aml-level = "v3";
                aml-type = "bl30";

                /* sign the binary contaiing bl30 and bl301 */
                aml-input {
                    type = "aml-encrypt";
                    aml-algo = "g12a";
                    aml-op = "bl30sig";
                    aml-level = "v3";

                    /*
                     * put bl30 and bl301 together, with
                     * the necessary paddiung
                     */
                    aml-input {
                        type = "section";
                        bl30 {
                            type = "blob-ext";
                            size = <0xa000>;
                            filename = "bl30.bin";
                        };
                        bl301 {
                            type = "blob-ext";
                            size = <0x3400>;
                            filename = "bl301.bin";
                        };
                    };
                };
            };

            /* sign the bl31 binary */
            bl31 {
                type = "aml-encrypt";
                aml-algo = "g12a";
                aml-op = "bl3sig";
                aml-input = "bl31.img";
                aml-level = "v3";
                aml-type = "bl31";
            };

            /* sign the bl33 binary (which is U-Boot) */
            bl33 {
                type = "aml-encrypt";
                aml-algo = "g12a";
                aml-op = "bl3sig";
                aml-compress = "lz4";
                aml-level = "v3";
                aml-type = "bl33";

                aml-input {
                    type = "u-boot";
                };
            };

            /* add the various DDR blobs */
            aml-ddrfw {
                missing-msg = "aml-ddrfw";
                type = "blob-ext-list";
                filenames = "ddr4_1d.fw", "ddr4_2d.fw",
                    "ddr3_1d.fw", "piei.fw",
                    "lpddr4_1d.fw", "lpddr4_2d.fw",
                    "diag_lpddr4.fw", "aml_ddr.fw",
                    "lpddr3_1d.fw";
            };
        };
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self._entries = OrderedDict()
        self.align_default = None
        self._aml_algo = None
        self._aml_op = None
        self._aml_level = None
        self.g12a = None
        self.g12b = None

    def ReadNode(self):
        super().ReadNode()
        self._aml_algo = fdt_util.GetString(self._node, 'aml-algo')
        self._aml_op = fdt_util.GetString(self._node, 'aml-op')
        self._aml_level = fdt_util.GetString(self._node, 'aml-level')
        self._aml_input = fdt_util.GetString(self._node, 'aml-input')
        self._aml_compress = fdt_util.GetString(self._node, 'aml-compress')
        self._aml_type = fdt_util.GetString(self._node, 'aml-type')
        self.ReadEntries()

    def ReadEntries(self):
        """Read the subnodes to find out what should go in this image"""
        for node in self._node.subnodes:
            etype = None
            if node.name.startswith('aml-') and 'type' not in node.props:
                etype = 'blob-ext'
            entry = Entry.Create(self, node, etype)
            entry.ReadNode()
            self._entries[entry.name] = entry

    def BuildSectionData(self, required):
        uniq = self.GetUniqueName()
        output_fname = tools.GetOutputFilename('aml-out.%s' % uniq)
        args = [f'--{self._aml_op}', '--output', output_fname]
        if self._aml_level:
            args += ['--level', f'{self._aml_level}']
        if self._aml_compress:
            args += ['--compress', f'{self._aml_compress}']
        if self._aml_type:
            args += ['--type', f'{self._aml_type}']
        if self._aml_input:
            input_pathname = tools.GetInputFilename(
                self._aml_input,
                self.section.GetAllowMissing())
            if not input_pathname:
                missing = True
                input_pathname = self.check_fake_fname(self._aml_input)
            args += ['--input', f'{input_pathname}']

        missing = False
        for entry in self._entries.values():
            # First get the input data and put it in a file. If not available,
            # try later.
            entry_data = entry.GetData(required)
            if not required and entry_data is None:
                return None
            flag_name = entry.name.replace('aml-', '')  # Drop the aml- prefix
            if isinstance(entry, Entry_blob_ext_list):
                for i, pathname in enumerate(entry._pathnames):
                    args += [f'--{flag_name}{i + 1}', pathname]
            elif isinstance(entry, Entry_blob_ext):
                pathname = entry._pathname
                args += [f'--{flag_name}', pathname]
            else:
                data = self.GetPaddedDataForEntry(entry, entry_data)
                fname = tools.GetOutputFilename('aml-in.%s' %
                                                entry.GetUniqueName())
                tools.WriteFile(fname, data)
                args += [f'--{flag_name}', fname]
            if entry.missing:
                missing = True

        if missing:
            self.missing = True
            return b''

        tout.Debug(f"Node '{self._node.path}': running: %s" % ' '.join(args))
        to_run = self.g12a if self._aml_algo == 'g12a' else self.g12b

        out = to_run.run_cmd(*args)

        # If an input file (or subnode!) is providing the input, the tools
        # writes to the requested output file. Otherwise it uses the output file
        # as a template for three files that it writes, ending in '.sd.bin',
        # 'usb.bl2' and 'usb.tpl'. We use the first one as the image output
        if self._aml_input or self._node.FindNode('aml-input'):
            real_outfile = output_fname
        else:
            real_outfile = f'{output_fname}.sd.bin'
        if out is not None:
            data = tools.ReadFile(real_outfile)
        else:
            data = tools.GetBytes(0, 1024)
        return data

    def SetImagePos(self, image_pos):
        Entry.SetImagePos(self, image_pos)

    def SetCalculatedProperties(self):
        Entry.SetCalculatedProperties(self)

    def CheckEntries(self):
        Entry.CheckEntries(self)

    def AddBintools(self, tools):
        self.g12a = self.AddBintool(tools, 'aml_encrypt_g12a')
        self.g12b = self.AddBintool(tools, 'aml_encrypt_g12b')
