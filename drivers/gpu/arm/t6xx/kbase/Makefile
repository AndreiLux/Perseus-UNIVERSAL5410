#
# (C) COPYRIGHT 2011-2012 ARM Limited. All rights reserved.
#
# This program is free software and is provided to you under the terms of the GNU General Public License version 2
# as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
#
# A copy of the licence is included with the program, and can also be obtained from Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
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
UMP_PATH=$(MROOT)/kernel/drivers/base/ump
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

