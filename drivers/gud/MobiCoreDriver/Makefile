#
# this makefile is called from the kernel make system. Thus we basiclly
# add things to "obj-m" here.

ifeq ($(MODE),release)
    EXTRA_CFLAGS = -O2 -DNDEBUG
else
    EXTRA_CFLAGS = -DDEBUG
endif # DEBUG/RELEASE

# CFLAGS from the build script
ifdef MOBICORE_CFLAGS
    EXTRA_CFLAGS+=$(MOBICORE_CFLAGS)
endif
#EXTRA_CFLAGS+=-DDEBUG_VERBOSE

EXTRA_CFLAGS += -include $(M)/build_tag.h
# add our module to kernel.
obj-m += mcDrvModule.o

mcDrvModule-objs :=logging.o main.o

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.markers Module.symvers modules.order

