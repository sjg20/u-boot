.. SPDX-License-Identifier: GPL-2.0+

U-Boot for ODROID-C4
====================

ODROID-C4 is a single board computer manufactured by Hardkernel
Co. Ltd with the following specifications:

 - Amlogic S905X3 Arm Cortex-A55 quad-core SoC
 - 4GB DDR4 SDRAM
 - Gigabit Ethernet
 - HDMI 2.1 display
 - 40-pin GPIO header
 - 4x USB 3.0 Host
 - 1x USB 2.0 Host/OTG (micro)
 - eMMC, microSD
 - UART serial
 - Infrared receiver

The ODROID-HC4 is a variant with a PCIe-SATA controller, the same commands
applies for HC4.

Schematics are available on the manufacturer website.

Setting up binary blobs
-----------------------

Amlogic doesn't provide sources for the firmware and for tools needed
to create the bootloader image, so it is necessary to obtain them from
the git tree published by the board vendor:

.. code-block:: bash

    # This may be needed with this older U-Boot release
    apt remove libfdt-dev

    wget https://releases.linaro.org/archive/13.11/components/toolchain/binaries/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux.tar.xz
    wget https://releases.linaro.org/archive/13.11/components/toolchain/binaries/gcc-linaro-arm-none-eabi-4.8-2013.11_linux.tar.xz
    tar xvfJ gcc-linaro-aarch64-none-elf-4.8-2013.11_linux.tar.xz
    tar xvfJ gcc-linaro-arm-none-eabi-4.8-2013.11_linux.tar.xz
    export PATH=$PWD/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux/bin:$PWD/gcc-linaro-arm-none-eabi-4.8-2013.11_linux/bin:$PATH

    DIR=odroid-c4
    git clone --depth 1 \
       https://github.com/hardkernel/u-boot.git -b odroidg12-v2015.01 \
       $DIR

    cd odroid-c4
    make odroidc4_defconfig
    make
    export UBOOTDIR=$PWD

U-Boot compilation
------------------

Go back to mainline U-Boot source tree then :

.. code-block:: bash

    $ export CROSS_COMPILE=aarch64-none-elf-
    $ make odroid-c4_defconfig
    $ BINMAN_TOOLPATHS=$UBOOTDIR/odroid-c4/fip/g12a \
      BINMAN_INDIRS="$UBOOTDIR/fip/g12a \
      $UBOOTDIR/build/board/hardkernel/odroidc4/firmware \
      $UBOOTDIR/build/scp_task" make

and then write the image to SD with:

.. code-block:: bash

    DEV=/dev/your_sd_device
    dd if=image.bin of=$DEV conv=fsync,notrunc

If you copy the `$UBOOTDIR/odroid-c4/fip/g12a/aml_encrypt_g12a` tool somewhere
in your path, you can omit the `BINMAN_TOOLPATHS` option. The `BINMAN_INDIRS`
variable provides a space-seperated list of directories containing the various
binary blobs needed by the build.

To see these, look at the Binman image description in
`arch/arm/dts/meson-sm1-odroid-c4-u-boot.dtsi`.
