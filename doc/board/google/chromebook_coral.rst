.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Simon Glass <sjg@chromium.org>

Chromebook Coral
================

Here are some random notes, to be expanded.

Boot flow - TPL
---------------

Apollolake boots via an IFWI (Integrated Firmware Image). TPL is placed in this,
in the IBBL entry.

On boot, an on-chip microcontroller called the CSE (Converged Security Engine)
sets up some SDRAM at ffff8000 and loads the TPL image to that address. The
SRAM extends up to the top of 32-bit address space, but the last 2KB is the
start16 region, so the TPL image must be 30KB at most, and CONFIG_TPL_TEXT_BASE
must be ffff8000. Actually the start16 region is small and it could probably
move from f800 to fe00, providing another 1.5KB, but TPL is only about 23KB so
there is no need to change it at present.

TPL (running from start.S) first sets up CAR (Cache-as-RAM) which provides
larger area of RAM for use while booting. CAR is mapped at CONFIG_SYS_CAR_ADDR
(fef00000) and is 768KB in size. It then sets up the stack in the botttom 64KB
of this space (i.e. below fef10000). This means that the stack and early
malloc() region in TPL can be 64KB at most.

TPL operates without CONFIG_TPL_PCI enabled so PCI config access must use the
x86-specific functions pci_x86_write_config(), etc. SPL creates a simple-bus
device so that PCI devices are bound by driver model. Then arch_cpu_init_tpl()
is called to early init on various devices. This includes placing PCI devices
at hard-coded addresses in the memory map. PCI auto-config is not used.

Most of the 16KB ROM is mapped into the very top of memory, except for the
Intel descriptor (first 4KB) and the space for SRAM as above.

TPL does not set up a bloblist since at present it does not have anything to
pass to SPL.

Once TPL is done it loads SPL from ROM using either the memory-mapped SPI or by
using the Intel fast SPI driver. SPL is loaded into CAR, at the address given
by CONFIG_SPL_TEXT_BASE, which is normally fef10000.


Boot flow - SPL
---------------

SPL (running from start_from_tpl.S) continues to use the same stack as TPL.
It calls arch_cpu_init_spl() to set up a few devices, then init_dram() loads
the FSP-M binary into CAR and runs to, to set up SDRAM. The address of the
output 'HOB' list (Hand-off-block) is stored into gd->arch.hob_list for parsing.
There is a 2GB chunk of SDRAM starting at 0 and the rest is at 4GB.

PCI auto-config is not used in SPL either, but CONFIG_SPL_PCI is defined, so
proper PCI access is available and normal dm_pci_read_config() calls can be
used. However PCI auto-config is not used so the same static memory mapping set
up by TPL is still active.

SPL on x86 always runs with CONFIG_SPL_SEPARATE_BSS=y and BSS is at 120000
(see u-boot-spl.lds).

SPL sets up a bloblist and passes the SPL hand-off information to U-Boot proper.
This includes a pointer to the HOB list as well as DRAM information. See
struct arch_spl_handoff. The bloblist address is set by CONFIG_BLOBLIST_ADDR,
normally 100000.

Once SPL is finished it loads U-Boot into SDRAM at CONFIG_SYS_TEXT_BASE, which
is normally 1110000. Note that CAR is still active.


Boot flow - U-Boot pre-relocation
---------------------------------

U-Boot (running from start_from_spl.S) starts running in RAM and uses the same
stack as SPL. It does various init activities before relocation. Notably
arch_cpu_init_dm() sets up the pin muxing for the chip using a very large table
in the device tree.

PCI auto-config is not used before relocation, but CONFIG_PCI of course is
defined, so proper PCI access is available. The same static memory mapping set
up by TPL is still active until relocation.

As per usual, U-Boot allocates memory at the top of available RAM (a bit below
2GB in this case) and copies things there ready to relocate itself. Notably
reserve_arch() does not reserve space for the HOB list returned by FSP-M since
this is already located in RAM.

U-Boot then shuts down CAR and jumps to its relocated version.


Boot flow - U-Boot post-relocation
---------------------------------

U-Boot starts up normally, running near the top of RAM. After driver model is
running, arch_fsp_init_r() is called which loads and runs the FSP-S binary.
This updates the HOB list to include graphics information, used by the fsp_video
driver.

PCI autoconfig is done and a few devices are probed to complete init. Most
others are started only when they are used.

Note that FSP-S is supposed to run after CAR has been shut down, which happens
immediately before U-Boot started up in its relocated position. Therefore we
cannot run FSP-S before relocation. On the other hand we must run it before
PCI auto-config is done, since FSP-S may show or hide devices. The first device
that probes PCI after relocation is the serial port, in initr_serial(), so FSP-S
must run before that.

It would be possible to tear down CAR in SPL instead of U-Boot. The SPL handoff
information could make sure it does not include any pointers into CAR (in fact
it doesn't). Doing this in U-Boot allows the initial state used by TPL and SPL
to be read by U-Boot, which seems useful. It also matches how older platforms
start up (those that don't use SPL).



Partial ROM map

fef07000	TPL/SPL Stack top
fef10000
fef16000 2a000	FSP M default stack
fef40000	SPL
fef71000 59000	FSP M
fefca000

Partial memory map

CONFIG_BLOBLIST_ADDR=0x100000


[1] Intel PDF https://www.coreboot.org/images/2/23/Apollolake_SoC.pdf
