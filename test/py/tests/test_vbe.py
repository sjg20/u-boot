# SPDX-License-Identifier:	GPL-2.0+
# Copyright 2022 Google LLC
#
# Test addition of VBE

import pytest

import common

# Define a base ITS which we can adjust using % and a dictionary
base_its = '''
/dts-v1/;

/ {
        description = "Example kernel";

        images {
            kernel-1 {
                data = /incbin/("%(kernel)s");
                type = "kernel";
                arch = "sandbox";
                os = "linux";
                load = <0x40000>;
                entry = <0x8>;
                compression = "%(compression)s";

                random {
                    compatible = "vbe,random-bytes";
                    vbe,bytes = <0x40>;
                    vbe,required;
                };
                aslr1 {
                    compatible = "vbe,aslr-move";
                    vbe,align = <0x100000>;
                };
                aslr2 {
                    compatible = "vbe,aslr-bytes";
                };
                wibble {
                    compatible = "vbe,wibble";
                };
            };

            fdt-1 {
                description = "snow";
                data = /incbin/("%(fdt)s");
                type = "flat_dt";
                arch = "sandbox";
                load = <%(fdt_addr)#x>;
                compression = "%(compression)s";
            };
        };
        configurations {
            default = "conf-1";
            conf-1 {
                kernel = "kernel-1";
                fdt = "fdt-1";
            };
        };
};
'''

# Define a base FDT - currently we don't use anything in this
base_fdt = '''
/dts-v1/;

/ {
    chosen {
    };
};
'''

# This is the U-Boot script that is run for each test. First load the FIT,
# then run the 'bootm' command, then save out memory from the places where
# we expect 'bootm' to write things. Then quit.
base_script = '''
host load hostfs 0 %(fit_addr)x %(fit)s
fdt addr %(fit_addr)x
bootm start %(fit_addr)x
bootm loados
fdt print
ut bootstd vbe_test_fixup
'''

@pytest.mark.boardspec('sandbox')
@pytest.mark.requiredtool('dtc')
def test_vbe(u_boot_console):
    cons = u_boot_console
    kernel = common.make_kernel(cons, 'vbe-kernel.bin', 'kernel')
    fdt = common.make_dtb(cons, base_fdt, 'vbe-fdt')
    fdt_out = common.make_fname(cons, 'fdt-out.dtb')

    params = {
        'fit_addr' : 0x1000,

        'kernel' : kernel,

        'fdt' : fdt,
        'fdt_out' : fdt_out,
        'fdt_addr' : 0x80000,
        'fdt_size' : 0x1000,

        'compression' : 'none',
    }
    mkimage = cons.config.build_dir + '/tools/mkimage'
    fit = common.make_fit(cons, mkimage, base_its, params, 'test-vbe.fit',
                          base_fdt)
    params['fit'] = fit
    cmd = base_script % params

    with cons.log.section('Kernel load'):
        output = cons.run_command_list(cmd.splitlines())

    #load fdt_out
