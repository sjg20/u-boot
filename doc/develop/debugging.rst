.. SPDX-License-Identifier: GPL-2.0+
.. Copyright (c) 2020 Heinrich Schuchardt

Debugging
=========

This describes a few debugging techniques for different parts of U-Boot.

Makefiles
---------

You can use $(warning) to show debugging information in a makefile::

   $(warning SPL: $(CONFIG_SPL_BUILD) . $(SPL_TPL_))

When make executes these they produce a message. If you put them in a rule, they
are executed when the rule is executed. For example, to show the value of a
variable at the point where it is used::

   tools-only: scripts_basic $(version_h) $(timestamp_h) tools/version.h
      $(warning version_h: $(version_h))
      $(Q)$(MAKE) $(build)=tools

You can use ifndef in makefiles for simple CONFIG checks::

   ifndef CONFIG_DM_DEV_READ_INLINE
   obj-$(CONFIG_OF_CONTROL) += read.o
   endif

but for those which require variable expansion you should use ifeq or ifneq::

   ifeq ($(CONFIG_$(SPL_TPL_)TINY_ONLY),)
   obj-y	+= device.o fdtaddr.o lists.o root.o uclass.o util.o
   endif

