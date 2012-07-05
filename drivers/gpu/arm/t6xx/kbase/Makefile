# Copyright:
# ----------------------------------------------------------------------------
# This confidential and proprietary software may be used only as authorized
# by a licensing agreement from ARM Limited.
#      (C) COPYRIGHT 2011-2012 ARM Limited, ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorized copies and
# copies may only be made to the extent permitted by a licensing agreement
# from ARM Limited.
# ----------------------------------------------------------------------------
#

# Defaults
export MALI_KBASE_SCHEDULING_POLICY?=cfs
export PLATFORM?=dummy
export MALI_BACKEND_KERNEL?=1
export MALI_NO_MALI?=0
export MALI_USE_UMP?=1
export MALI_DEBUG?=0
export MALI_ERROR_INJECT_ON?=0
export MALI_ANDROID?=0
export MALI_BASE_TRACK_MEMLEAK?=0
export MALI_KERNEL_TEST_API?=0
export MALI_UNIT_TEST?=0
export MALI_INFINITE_CACHE?=0
export MALI_LICENSE_IS_GPL?=1
export MALI_FAKE_PLATFORM_DEVICE?=0
export MALI_MOCK_TEST?=0
export MALI_UNCACHED?=0
export MALI_INSTRUMENTATION_LEVEL?=0
export MALI_CUSTOMER_RELEASE?=1
export MALI_GATOR_SUPPORT?=0
export MALI_COVERAGE?=0
export MALI_KBASEP_MODEL?=0

ifeq ($(MALI_PLATFORM_CONFIG), exynos5)
export MALI_UNCACHED=1
endif

KDIR ?= /lib/modules/$(shell uname -r)/build

# MROOT defines root of kernel device driver components
# This makefile may reside in container directory of the kernel device driver components or within the
# 'kbase' kernel driver component itself, so we set MROOT accordingly.
#
ifeq ($(wildcard $(PWD)/kbase/src/Makefile),)
MROOT=$(PWD)/../../../../../..
else
MROOT=$(PWD)
endif

KBASE_PATH=$(MROOT)/kernel/drivers/gpu/arm/t6xx/kbase
UMP_PATH=$(MROOT)/kernel/drivers/gpu/arm/ump
KDS_PATH=$(MROOT)/kernel
OSK_PATH=$(MROOT)/kernel/drivers/gpu/arm/t6xx/kbase/osk

all:
	$(MAKE) -C $(OSK_PATH)/src/linux
	$(MAKE) -C $(KDS_PATH)/lib/kds
	$(MAKE) -C $(UMP_PATH)/src/devicedrv
	$(MAKE) -C $(KBASE_PATH)/src

clean:
	$(MAKE) -C $(OSK_PATH)/src/linux clean
	$(MAKE) -C $(KDS_PATH)/lib/kds clean
	$(MAKE) -C $(UMP_PATH)/src/devicedrv clean
	$(MAKE) -C $(KBASE_PATH)/src clean

