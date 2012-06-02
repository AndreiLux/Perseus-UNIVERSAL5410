# Copyright:
# ----------------------------------------------------------------------------
# This confidential and proprietary software may be used only as authorized
# by a licensing agreement from ARM Limited.
#      (C) COPYRIGHT 2010-2011 ARM Limited, ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorized copies and
# copies may only be made to the extent permitted by a licensing agreement
# from ARM Limited.
# ----------------------------------------------------------------------------
#

ifneq ($(KERNELRELEASE),)

ROOT=$(KBUILD_EXTMOD)/../../..

SRC=\
	mali_osk_debug.c \
	mali_osk_workq.c \
	../common/mali_osk_bitops_cmn.c \
	../common/mali_osk_debug_cmn.c

ifeq ($(MALI_BASE_TRACK_MEMLEAK), 1)
SRC += ../common/mali_osk_failure.c ../common/mali_osk_mem_track.c
endif

ifeq ($(MALI_LICENSE_IS_GPL), 1)
SRC += mali_osk_timers.c
# ensure GPL version of malisw gets pulled in
EXTRA_CFLAGS += -I$(ROOT)/kbase
endif

lib-y := $(SRC:.c=.o)

include $(KBUILD_EXTMOD)/Makefile.osk

EXTRA_CFLAGS += -DMALI_DEBUG=$(MALI_DEBUG) -DMALI_BASE_TRACK_MEMLEAK=$(MALI_BASE_TRACK_MEMLEAK) -DMALI_UNIT_TEST=$(MALI_UNIT_TEST) -DMALI_LICENSE_IS_GPL=$(MALI_LICENSE_IS_GPL) -DMALI_USE_UMP=$(MALI_USE_UMP)

else

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR)

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

endif
