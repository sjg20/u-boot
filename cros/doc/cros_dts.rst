.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2020 Google LLC

Devicetree
==========

U-Boot uses devicetree to specify the devices in a system and to provide
run-time configuration information. With vboot, the standard devicetree is used
for a board, augmented with addition information.


Files
-----

The following files are used from cros/dts, #included in the board's .dts
file. All of these are built into one binary with the devicetree compiler:

chromeos-<arch>.dtsi
   Basic configuration for the architecture (e.g. x86, sandbox)

chromeos-<board>.dtsi
   Additional details specific to the board

chromeos-<arch>-ro.dtsi
   Binman definition for read-only part of the image

chromeos-<size>-<arch>-rw.dtsi
   Binman definition for read-write part of the image, where <size> is the
   image size (e.g. '16mb' means 16MiB)

factory-friendly.dtsi
   Sets up GBB flags for convenience in the factory. This should not be included
   for production boards.


Flash layout
------------

U-Boot uses a tool called binman to build and manage firmware images. The vboot
implementation uses binman to generate an `image.bin` file which includes the
full image:

   - read-only section
   - read/write section A
   - read/write section B
   - other read/write pieces like VPD, environment

The `image.bin` image is defined by the `binman/image` node in the devicetree.
Most nodes represent a file that is collected from the build output and placed
in the image.

Most boards already have a binman configuration, producing a `u-boot.rom` file.
This is left alone as it is still useful to generate this for testing purposes.

Binman generates an fdtmap which it uses to be able to decode the contents of
an image. It also created a simpler FMAP section which is used by some Chromium
OS tools.


Special devices
---------------

There are some special Chrome OS devices which are set up by the devicetree.
These are modelled as devices partly for ease of implementation, but also so it
is easy to see the system configuration just by looking at the devicetree.
Each one of these corresponds to a uclass created for Chromium OS:

   - nvdata - (`nvdata/` directory) various device for handling data stored
     between boots, such as the non-secure vboot context data, vboot's secure
     data and firmware hash. Each is a subnode of the device which holds the
     data. For example, if the EC is used to hold nvdata, the device is a
     sub-node of cros-ec

   - vboot-flag - (`flag/` directory) several flag values affect how vboot runs,
     such as whether or not the lid is open. These flags can come from GPIOs,
     switches, the EC, etc. From the point of view of vboot, all it cares about
     is getting the value.

   - fwstore - (`fwstore/` directory) provides access to the firmware image.
     This is typically a SPI-flash device but other options are possible, such
     as MMC

   - vboot-ec - while there is one main EC in a Chrome OS device, there are
     others as well, such as the USB Power Delivery chips. These use the same
     uclass and provide functions to allow them to be write-protected, updated
     and the like.

   - aux-fw - provides a way to check whether a device needs to have its
     firmware be updated and to update it if so
