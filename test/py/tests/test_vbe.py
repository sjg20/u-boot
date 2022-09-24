# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC

"""Test VBE OS requests are processed correctly

These should result in additions (fixups) to the device tree.
"""

import pytest

import fit_util

# Define a base ITS which we can adjust using % and a dictionary
BASE_ITS = '''
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
                    compatible = "vbe,random-rand";
                    vbe,size = <0x40>;
                    vbe,required;
                };
                aslr1 {
                    compatible = "vbe,aslr-move";
                    vbe,align = <0x100000>;
                };
                aslr2 {
                    compatible = "vbe,aslr-rand";
                };
                efi-runtime {
                    compatible = "vbe,efi-runtime-rand";
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
BASE_FDT = '''
/dts-v1/;

/ {
    chosen {
    };
};
'''

# This is the U-Boot script that is run for each test. First load the FIT,
# then run the 'bootm' command, then run the unit test which checks that the
# working tree has the required things filled in according to the OS requests
# above (random, aslr2, etc.)
BASE_SCRIPT = '''
host load hostfs 0 %(fit_addr)x %(fit)s
fdt addr %(fit_addr)x
bootm start %(fit_addr)x
bootm loados
bootm prep
fdt addr
fdt print
ut bootstd vbe_test_fixup
'''

@pytest.mark.boardspec('sandbox_flattree')
@pytest.mark.requiredtool('dtc')
def test_vbe(u_boot_console):
    """Set up a FIT, boot it and check the fixups happened"""
    cons = u_boot_console
    kernel = fit_util.make_kernel(cons, 'vbe-kernel.bin', 'kernel')
    fdt = fit_util.make_dtb(cons, BASE_FDT, 'vbe-fdt')
    fdt_out = fit_util.make_fname(cons, 'fdt-out.dtb')

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
    fit = fit_util.make_fit(cons, mkimage, BASE_ITS, params, 'test-vbe.fit',
                            BASE_FDT)
    params['fit'] = fit
    cmd = BASE_SCRIPT % params

    with cons.log.section('Kernel load'):
        output = cons.run_command_list(cmd.splitlines())

    # This is a little wonky since there are two tests running in CI. The final
    # one is the 'ut bootstd' command above
    failures = [line for line in output if 'Failures' in line]
    assert len(failures) >= 1 and 'Failures: 0' in failures[-1]
