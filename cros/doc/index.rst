.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2020 Google LLC

Internal Documentation
======================

This provides technical documentation on the U-Boot implementation of
Chromium OS's verified boot.

The implementation allows booting on certain Chrome OS devices (e.g. coral). It
also can run under :doc:`../../arch/sandbox` for development and testing
purposes.

.. toctree::
   :maxdepth: 2

   overview
   data
   cros_dts
   cros_sandbox
   cros_coral
