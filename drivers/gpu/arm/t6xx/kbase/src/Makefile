# Copyright:
# ----------------------------------------------------------------------------
# This confidential and proprietary software may be used only as authorized
# by a licensing agreement from ARM Limited.
#      (C) COPYRIGHT 2010-2012 ARM Limited, ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorized copies and
# copies may only be made to the extent permitted by a licensing agreement
# from ARM Limited.
# ----------------------------------------------------------------------------
#

KDIR ?= /lib/modules/$(shell uname -r)/build

UMP_PATH_RELATIVE = $(CURDIR)/../../../ump
KBASE_PATH_RELATIVE = $(CURDIR)/../../kbase
KDS_PATH_RELATIVE = $(CURDIR)/../../../../../..
EXTRA_CONFIGS =
USE_KDS =
EXTRA_SYMBOLS = $(UMP_PATH_RELATIVE)/src/devicedrv/Module.symvers

ifeq ($(MALI_UNIT_TEST), 1)
	EXTRA_SYMBOLS += $(KBASE_PATH_RELATIVE)/tests/internal/src/kernel_assert_module/linux/Module.symvers
endif

ifeq ($(findstring 0,$(MALI_LICENSE_IS_GPL)),)
# GPL driver supports KDS
	EXTRA_SYMBOLS += $(KDS_PATH_RELATIVE)/lib/kds/Module.symvers
	USE_KDS = -DCONFIG_KDS
endif

ifeq ($(MALI_NO_MALI), 1)
	EXTRA_CONFIGS += CONFIG_MALI_NO_MALI=y
endif

ifeq ($(MALI_BASE_TRACK_MEMLEAK),1)
	EXTRA_CONFIGS += CONFIG_MALI_BASE_TRACK_MEMLEAK=y
endif

ifeq ($(MALI_PLATFORM_CONFIG), vexpress)
	EXTRA_CONFIGS += CONFIG_MALI_PLATFORM_VEXPRESS=y
else
ifeq ($(MALI_PLATFORM_CONFIG), goldfish)
	EXTRA_CONFIGS += CONFIG_MALI_PLATFORM_GOLDFISH=y
else
ifeq ($(MALI_PLATFORM_CONFIG), pbx)
	EXTRA_CONFIGS += CONFIG_MALI_PLATFORM_PBX=y
else
	EXTRA_CONFIGS += CONFIG_MALI_PLATFORM_THIRDPARTY=y CONFIG_MALI_PLATFORM_THIRDPARTY_NAME="$(MALI_PLATFORM_CONFIG)"
endif # pbx
endif # goldfish
endif # vexpress

ifeq ($(MALI_USE_UMP), 1)
EXTRA_CONFIGS += CONFIG_UMP=y
endif

# we get the symbols from modules using KBUILD_EXTRA_SYMBOLS to prevent warnings about unknown functions
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) EXTRA_CFLAGS="-I$(CURDIR)/../../../../../../include $(USE_KDS)" CONFIG_MALI_T6XX=m $(EXTRA_CONFIGS) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

