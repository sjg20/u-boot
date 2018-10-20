# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2011 The Chromium OS Authors.

PLATFORM_CPPFLAGS += -D__SANDBOX__ -U_FORTIFY_SOURCE
PLATFORM_CPPFLAGS += -DCONFIG_ARCH_MAP_SYSMEM
PLATFORM_CPPFLAGS += -fPIC
PLATFORM_LIBS += -lrt
SDL_CONFIG ?= sdl-config

# Define this to avoid linking with SDL, which requires SDL libraries
# This can solve 'sdl-config: Command not found' errors
ifneq ($(NO_SDL),)
PLATFORM_CPPFLAGS += -DSANDBOX_NO_SDL
else
PLATFORM_LIBS += $(shell $(SDL_CONFIG) --libs)
PLATFORM_CPPFLAGS += $(shell $(SDL_CONFIG) --cflags)
endif

cmd_u-boot__ = $(CC) -o $@ -Wl,-T u-boot.lds \
	u-boot.o  $(u-boot-init) \
	$(PLATFORM_LIBS) -Wl,-Map -Wl,u-boot.map \

cmd_u-boot-spl = (cd $(obj) && $(CC) -o $(SPL_BIN) -Wl,-T u-boot-spl.lds \
	u-boot-spl.o $(patsubst $(obj)/%,%,$(u-boot-spl-init)) \
	$(PLATFORM_LIBS) -Wl,-Map -Wl,u-boot-spl.map -Wl,--gc-sections)

CONFIG_ARCH_DEVICE_TREE := sandbox
