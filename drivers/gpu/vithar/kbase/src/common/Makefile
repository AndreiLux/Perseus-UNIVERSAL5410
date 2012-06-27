ccflags-$(CONFIG_VITHAR) += -DMALI_DEBUG=0 -DMALI_HW_TYPE=2 \
	-DMALI_USE_UMP=0 -DMALI_HW_VERSION=r0p0 \
	-DMALI_BASE_TRACK_MEMLEAK=0 -DMALI_ANDROID=1 -DMALI_ERROR_INJECT_ON=0 \
	-DMALI_NO_MALI=0 -DMALI_BACKEND_KERNEL=1 \
	-DMALI_FAKE_PLATFORM_DEVICE=1 -DMALI_MOCK_TEST=0 -DMALI_KERNEL_TEST_API=0 \
	-DMALI_INFINITE_CACHE=0 -DMALI_LICENSE_IS_GPL=1 \
	-DMALI_PLATFORM_CONFIG=exynos5 -DUMP_SVN_REV_STRING="\"dummy\"" \
	-DMALI_RELEASE_NAME="\"dummy\"" -DMALI_UNIT_TEST=0 -DMALI_INSTRUMENTATION_LEVEL=0 -DMALI_CUSTOMER_RELEASE=1

ROOTDIR = $(src)/../../..

ccflags-$(CONFIG_VITHAR) += -I$(ROOTDIR)/kbase
ccflags-$(CONFIG_VITHAR) += -I$(src)/..

ccflags-y += -I$(ROOTDIR) -I$(ROOTDIR)/include -I$(ROOTDIR)/osk/src/linux/include -I$(ROOTDIR)/uk/platform_dummy
ccflags-y += -I$(ROOTDIR)/kbase/midg_gpus/r0p0

obj-y += mali_kbase_device.o
obj-y += mali_kbase_mem.o
obj-y += mali_kbase_mmu.o
obj-y += mali_kbase_jd.o
obj-y += mali_kbase_jm.o
obj-y += mali_kbase_gpuprops.o
obj-y += mali_kbase_js.o
obj-y += mali_kbase_js_affinity.o
obj-y += mali_kbase_pm.o
obj-y += mali_kbase_event.o
obj-y += mali_kbase_context.o
obj-y += mali_kbase_pm.o
obj-y += mali_kbase_pm_driver.o
obj-y += mali_kbase_pm_metrics.o
obj-y += mali_kbase_pm_always_on.o
obj-y += mali_kbase_pm_demand.o
obj-y += mali_kbase_config.o
obj-y += mali_kbase_security.o
obj-y += mali_kbase_instr.o
#obj-y += mali_kbase_instr_7115.o
obj-y += mali_kbase_cpuprops.o
obj-y += mali_kbase_js_ctx_attr.o
obj-y += mali_kbase_8401_workaround.o
obj-y += mali_kbase_cache_policy.o
obj-y += mali_kbase_softjobs.o

obj-y += mali_kbase_pm_metrics_dummy.o

obj-y += mali_kbase_js_policy_cfs.o
obj-y += mali_kbase_hw.o
#obj-y += mali_kbase_js_policy_fcfs.o
