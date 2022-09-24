vbe command
===========

Synopsis
--------

::

    vbe list
    vbe select <name_or_id>
    vbe state

Description
-----------

The vbe command provides a way to interact with :doc:`../../develop/vbe`. It has
subcommands for different purposes.

vbe list
~~~~~~~~

Lists the available VBE bootmeths. These are a subset of all bootmeths, as
accessed via the :doc:`bootmeth`.

The fields are as follows:

#:
    Shows the bootmeth sequence number, Use the `bootmeth list` command to see
    all available bootmeths.

Sel:
    Indicates the selected bootmeth with an asterisk (`*`).

Device:
    Name of VBE device, which is taken from the name of its device tree node.

Driver:
    Name of the VBE driver

Description:
    Description of the VBE driver


vbe select
~~~~~~~~~~

Allows a particular bootmeth to be selected. Either a sequence number or a
device name can be provided.

Without any arguments, any selected device is deselected.


vbe state
~~~~~~~~~

This shows the current state of VBE. At present this is just a list of the
U-Boot phases which booted using VBE.


Examples
--------

This shows listing and selecting devices::

    => vbe list
      #  Sel  Device           Driver          Description
    ---  ---  --------------   --------------  -----------
      2       firmware0        vbe_simple      VBE simple
    ---  ---  --------------   --------------  -----------
    => vbe sel 2
    => vbe list
      #  Sel  Device           Driver          Description
    ---  ---  --------------   --------------  -----------
      2  *    firmware0        vbe_simple      VBE simple
    ---  ---  --------------   --------------  -----------
    => vbe sel
    => vbe list
      #  Sel  Device           Driver          Description
    ---  ---  --------------   --------------  -----------
      2       firmware0        vbe_simple      VBE simple
    ---  ---  --------------   --------------  -----------

This shows selecting a VBE device by its name::

    => vbe sel firmware0
    => vbe list
      #  Sel  Device           Driver          Description
    ---  ---  --------------   --------------  -----------
      2  *    firmware0        vbe_simple      VBE simple
    ---  ---  --------------   --------------  -----------
    =>

This shows the state after a successful boot into U-Boot proper, using VBE::

   => vbe state
   Phases: VPL SPL


Return value
------------

The return value $? is set to 0 (true) on success, or 1 (failure) if something
goes wrong, such as failing to find the bootmeth with `vbe select`.
