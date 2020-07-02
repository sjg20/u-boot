.. SPDX-License-Identifier: GPL-2.0+

Tiny driver model (tiny-dm)
===========================

Purpose
-------

Reduce the overhead of using driver model in SPL and TPL.


Introduction
------------

On some platforms that use SPL [1]_, SRAM is extremely limited. There is a
need to use as little space as possible for U-Boot SPL.

With the migration to driver model and devicetree, the extra software complexity
has created more pressure on U-Boot's code and data size.

A few features have been introduced to help with this problem:

  - fdtgrep, introduced in 2015, automatically removes unnecessary parts of the
    device tree, e.g. those used by drivers not present in SPL. At the time,
    this typically reduced SPL size from about 40KB to perhaps 3KB and made
    it feasible to look at using driver model with SPL. The minimum overhead
    was reduced to approximately 7KB on Thumb systems, for example [2]_
  - of-platdata, introduced in 2016 [3]_, converts the device tree into C data
    structures which are placed in the SPL image. This saves approximately
    3KB of code and replaces the devicetree with something typically 30%
    smaller.

However the problem still exists. Even with of-platdata, the driver-model
overhead is typically 3KB at the minimum. This excludes the size of allocated
data structures, which is 84 bytes per device and 76 bytes per uclass on
32-bit machines. On 64-bit machines these sizes approximately double.

With the driver-model migration deadlines passing, a solution is needed to
allow boards to complete migration to driver model in SPL, without taking on
the full ~5KB overhead that this entails.


Concept
-------

The idea of tiny devices ('tiny-dm') builds on of-platdata, but additionally
removes most of the rich feature-set of driver model.

In particular tiny-dm takes away the concept of a uclass (except that it stil
uses uclass IDs), drastically reduces the size of a device (to 16 bytes on
32-bit) and removes the need for a driver_info structure.

With tiny-dm, dtoc outputs U_BOOT_TINY_DEVICE() instead of U_BOOT_DEVICE().
A new 'struct tiny_dev' is used instead of 'struct udevice'. Devices can be
located based on uclass ID and sequence number with tiny_dev_find(). Devices can
be probed with tiny_dev_probe().

In fact, tiny-dm is effectively a bypass for most of driver model. It retains
some capability with in (chiefly by using the same device tree), but new code
is added to implement simple features in a simple way.

Tiny-dm is not suitable for complex device and interactions, but it can
support a serial port (output only), I2C buses and other features needed to
set up the machine just enough to load U-Boot proper.

It is possible to enable Tiny-dm on a subsystem-by-subsystem basis. For example,
enabling CONFIG_TPL_TINY_SERIAL on chromebook_coral saves about 900 bytes of
code and data, with no perceptable difference in operation.


Tiny devices
------------

Below is an example of a tiny device, a UART that uses NS16550. It works by
setting up a platform structure to pass to the ns16550 driver, perhaps the
worst driver in U-Boot.

.. code-block:: c

    static int apl_ns16550_tiny_probe(struct tiny_dev *tdev)
    {
            struct dtd_intel_apl_ns16550 *dtplat = tdev->dtplat;
            struct ns16550_platdata *plat = tdev->priv;
            ulong base;
            pci_dev_t bdf;

            base = dtplat->early_regs[0];
            bdf = pci_ofplat_get_devfn(dtplat->reg[0]);

            if (!CONFIG_IS_ENABLED(PCI))
                    apl_uart_init(bdf, base);

            plat->base = base;
            plat->reg_shift = dtplat->reg_shift;
            plat->reg_width = 1;
            plat->clock = dtplat->clock_frequency;
            plat->fcr = UART_FCR_DEFVAL;

            return ns16550_tiny_probe_plat(plat);
    }

    static int apl_ns16550_tiny_setbrg(struct tiny_dev *tdev, int baudrate)
    {
            struct ns16550_platdata *plat = tdev->priv;

            return ns16550_tiny_setbrg(plat, baudrate);
    }

    static int apl_ns16550_tiny_putc(struct tiny_dev *tdev, const char ch)
    {
            struct ns16550_platdata *plat = tdev->priv;

            return ns16550_tiny_putc(plat, ch);
    }

    struct tiny_serial_ops apl_ns16550_tiny_ops = {
            .probe	= apl_ns16550_tiny_probe,
            .setbrg	= apl_ns16550_tiny_setbrg,
            .putc	= apl_ns16550_tiny_putc,
    };

    U_BOOT_TINY_DRIVER(apl_ns16550) = {
            .uclass_id	= UCLASS_SERIAL,
            .probe		= apl_ns16550_tiny_probe,
            .ops		= &apl_ns16550_tiny_ops,
            DM_TINY_PRIV(<ns16550.h>, sizeof(struct ns16550_platdata))
    };

The probe function is responsible for setting up the hardware so that the UART
can output characters. This driver enables the device on PCI and assigns an
address to its BAR (Base-Address Register). That code is in apl_uart_init() and
is not show here. Then it sets up a platdata data structure for use by the
ns16550 driver and calls its probe function.

The 'tdev' device is declared like this in the device tree:

.. code-block:: c

    serial: serial@18,2 {
        reg = <0x0200c210 0 0 0 0>;
        u-boot,dm-pre-reloc;
        compatible = "intel,apl-ns16550";
        early-regs = <0xde000000 0x20>;
        reg-shift = <2>;
        clock-frequency = <1843200>;
        current-speed = <115200>;
    };

When dtoc runs it outputs the following code for this, into dt-platdata.c:

.. code-block:: c

    static struct dtd_intel_apl_ns16550 dtv_serial_at_18_2 = {
            .clock_frequency	= 0x1c2000,
            .current_speed	= 0x1c200,
            .early_regs		= {0xde000000, 0x20},
            .reg		= {0x200c210, 0x0},
            .reg_shift		= 0x2,
    };

    DM_DECL_TINY_DRIVER(apl_ns16550);
    #include <ns16550.h>
    u8 _serial_at_18_2_priv[sizeof(struct ns16550_platdata)] __attribute__ ((section (".data")));
    U_BOOT_TINY_DEVICE(serial_at_18_2) = {
            .dtplat		= &dtv_serial_at_18_2,
            .drv		= DM_REF_TINY_DRIVER(apl_ns16550),
            .priv		= _serial_at_18_2_priv,
    };

This basically creates a device, with a pointer to the dtplat data (a C
structure similar to the devicetree node) and a pointer to the driver, the
U_BOOT_TINY_DRIVER() thing shown above.

So far, tiny-dm might look pretty similar to the full driver model, but there
are quite a few differences that may not be immediately apparent:

   - Whereas U_BOOT_DEVICE() emits a driver_info structure and then allocates
     the udevice structure at runtime, U_BOOT_TINY_DEVICE() emits an actual
     tiny_dev device structure into the image. On platforms where SPL runs in
     read-only memory, U-Boot automatically copies this into RAM as needed.
   - The DM_TINY_PRIV() macro tells U-Boot about the private data needed by
     the device. But this is not allocated at runtime. Instead it is declared
     in the C structure above. However on platforms where SPL runs in read-only
     memory, allocation is left until runtime.
   - There is a corresponding 'full' driver in the same file with the same name.
     Like of-platdata, it is not possible to use tiny-dm without 'full' support
     added as well. This makes sense because the device needs to be supported
     in U-Boot proper as well.
   - While this driver is in the UCLASS_SERIAL uclass, there is in fact no
     uclass available. The serial-uclass.c implementation has an entirely
     separate (small) piece of code to support tiny-dm:

.. code-block:: c

    int serial_init(void)
        {
            struct tiny_dev *tdev;
            int ret;

            tdev = tiny_dev_find(UCLASS_SERIAL, 0);
            if (!tdev) {
                    if (IS_ENABLED(CONFIG_REQUIRE_SERIAL_CONSOLE))
                            panic_str("No serial");
                    return -ENODEV;
            }
            ret = tiny_dev_probe(tdev);
            if (ret)
                    return log_msg_ret("probe", ret);
            gd->tiny_serial = tdev;
            gd->flags |= GD_FLG_SERIAL_READY;
            serial_setbrg();

            return 0;
        }

        void serial_putc(const char ch)
        {
            struct tiny_dev *tdev = gd->tiny_serial;
            struct tiny_serial_ops *ops;

            if (!tdev)
                    goto err;

            ops = tdev->drv->ops;
            if (!ops->putc)
                    goto err;
            if (ch == '\n')
                    ops->putc(tdev, '\r');
            ops->putc(tdev, ch);

            return;
        err:
            if (IS_ENABLED(DEBUG_UART))
                    printch(ch);
        }

        void serial_puts(const char *str)
        {
            for (const char *s = str; *s; s++)
                    serial_putc(*s);
        }


When serial_putc() is called from within U-Boot, this code looks up the tiny-dm
device and sends it the character.


Potential costs and benefits
----------------------------

It is hard to estimate the savings to be had by switching a subsystem over to
tiny-dm. Further work will illuminate this. In the example above (on x86),
about 1KB bytes is saved (code and data), but this may or may not be
representative of other subsystems.

If all devices in an image use tiny-dm then it is possible to remove all the
core driver-model support. This is the 3KB mentioned earlier. Of course, tiny-dm
has its own overhead, although it is substantialy less than the full driver
model.

These benefits come with some drawbacks:

   - Drivers that want to use it must implement tiny-dm in addition to their
     normal support.
   - of-platdata must be used. This cannot be made to work with device tree.
   - Tiny-dm drivers have none of the rich support provided by driver model.
     There is no pre-probe support, no concept of buses holding information
     about child devices, no automatic pin control or power control when a
     device is probed. Tiny-dm is designed to save memory, not to make it easy
     to write complex device drivers.
   - Subsystems must be fully migrated to driver model with the old code
     removed. This is partly a technical limitation (see ns16550.c for how ugly
     it is to support both, let alone three) and partly a quid-pro-quo for
     this feature, since it should remove existing concerns about migrating to
     driver model.


Next steps
----------

This is currently an RFC so the final result may change somewhat from what is
presented here. Some features are missing, in particular the concept of sequence
numbers is designed but not implemented. The code is extremely rough.

To judge the impact of tiny-dm a suitable board needs to be fully converted to
it. At present I am leaning towards rock2, since it already supports
of-platdata.

The goal is to sent initial patches in June 2020 with the first version in
mainline in July 2020 ready for the October release. Refinements based on
feedback and patches received can come after that. It isn't clear yet when this
could become a 'stable' feature, but likely after a release or two, perhaps with
5-10 boards converted.


Trying it out
-------------

The source tree is available at https://github.com/sjg20/u-boot/tree/dtoc-working

Only two boards are supported at present:

   - sandbox_spl - run spl/u-boot-spl to try the SPL with tiny-dm
   - chromebook_coral - TPL uses tiny-dm


.. [1] This discussion refers to SPL but for devices that use TPL, the same
       features are available there.
.. [2] https://www.elinux.org/images/c/c4/\Order_at_last_-_U-Boot_driver_model_slides_%282%29.pdf
.. [3] https://elinux.org/images/8/82/What%27s_New_with_U-Boot_%281%29.pdf


.. Simon Glass <sjg@chromium.org>
.. Google LLC
.. Memorial Day 2020
