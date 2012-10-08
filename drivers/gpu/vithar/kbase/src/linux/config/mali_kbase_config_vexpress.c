/*
 *
 * (C) COPYRIGHT 2011-2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#include <linux/ioport.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/src/linux/mali_kbase_config_linux.h>
#include <ump/ump_common.h>

#include "mali_kbase_cpu_vexpress.h"

/* Set this to 1 to enable dedicated memory banks */
#define T6F1_ZBT_DDR_ENABLED 0
#define HARD_RESET_AT_POWER_OFF 0

static kbase_io_resources io_resources =
{
	.job_irq_number   = 68,
	.mmu_irq_number   = 69,
	.gpu_irq_number   = 70,
	.io_memory_region =
	{
		.start = 0xFC010000,
		.end   = 0xFC010000 + (4096 * 5) - 1
	}
};

#if T6F1_ZBT_DDR_ENABLED

static kbase_attribute lt_zbt_attrs[] =
{
	{
		KBASE_MEM_ATTR_PERF_CPU,
		KBASE_MEM_PERF_SLOW
	},
	{
		KBASE_MEM_ATTR_END,
		0
	}
};

static kbase_memory_resource lt_zbt =
{
	.base = 0xFD000000,
	.size = 16 * 1024 * 1024UL /* 16MB */,
	.attributes = lt_zbt_attrs,
	.name = "T604 ZBT memory"
};


static kbase_attribute lt_ddr_attrs[] =
{
	{
		KBASE_MEM_ATTR_PERF_CPU,
		KBASE_MEM_PERF_SLOW
	},
	{
		KBASE_MEM_ATTR_END,
		0
	}
};

static kbase_memory_resource lt_ddr =
{
	.base = 0xE0000000,
	.size = 256 * 1024 * 1024UL /* 256MB */,
	.attributes = lt_ddr_attrs,
	.name = "T604 DDR memory"
};

#endif /* T6F1_ZBT_DDR_ENABLED */

static int pm_callback_power_on(kbase_device *kbdev)
{
	/* Nothing is needed on VExpress, but we may have destroyed GPU state (if the below HARD_RESET code is active) */
	return 1;
}

static void pm_callback_power_off(kbase_device *kbdev)
{
#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
#endif
}

static kbase_pm_callback_conf pm_callbacks =
{
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off
};

static kbase_attribute config_attributes[] =
{
	{
		KBASE_CONFIG_ATTR_MEMORY_PER_PROCESS_LIMIT,
		512 * 1024 * 1024UL /* 512MB */
	},
	{
		KBASE_CONFIG_ATTR_UMP_DEVICE,
		UMP_DEVICE_Z_SHIFT
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
		768 * 1024 * 1024UL /* 768MB */
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
		KBASE_MEM_PERF_SLOW
	},

#if T6F1_ZBT_DDR_ENABLED
	{
		KBASE_CONFIG_ATTR_MEMORY_RESOURCE,
		(uintptr_t)&lt_zbt
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_RESOURCE,
		(uintptr_t)&lt_ddr
	},
#endif /* T6F1_ZBT_DDR_ENABLED */

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
		5000
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
		5000
	},

#if MALI_DEBUG
/* Use more aggressive scheduling timeouts in debug builds for testing purposes */
	{
		KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
		15000000 /* 15ms, an agressive tick for testing purposes. This will reduce performance significantly */
	},

	{
		KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
		1 /* between 15ms and 30ms before soft-stop a job */
	},

	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
#if BASE_HW_ISSUE_8408 != 0
		2000 /* 30s before hard-stop, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) */
#else
		333 /* 5s before hard-stop */
#endif
	},

	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
		100000 /* 1500s (25mins) before NSS hard-stop */
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
#if BASE_HW_ISSUE_8408 != 0
		3000 /* 45s before resetting GPU, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) */
#else
		500 /* 7.5s before resetting GPU */
#endif
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
		100166 /* 1502s before resetting GPU */
	},
#else /* MALI_DEBUG */
/* In release builds same as the defaults but scaled for 5MHz FPGA */
	{
		KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
		2500000000u /* 2.5s */
	},

	{
		KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
		1 /* 2.5s before soft-stop a job */
	},

	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
#if BASE_HW_ISSUE_8408 != 0
		12 /* 30s before hard-stop, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) */
#else
		2 /* 5s before hard-stop */
#endif
	},

	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
		600 /* 1500s before NSS hard-stop */
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
#if BASE_HW_ISSUE_8408 != 0
		18 /* 45s before resetting GPU, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) */
#else
		3 /* 7.5s before resetting GPU */
#endif
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
		601 /* 1502s before resetting GPU */
	},
#endif /* MALI_DEBUG */
	{
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		3000 /* 3s before cancelling stuck jobs */
	},

	{
		KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS,
		1000000 /* 1ms - an agressive timeslice for testing purposes (causes lots of scheduling out for >4 ctxs) */
	},

	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
		(uintptr_t)&pm_callbacks
	},

	{
		KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
		(uintptr_t)&kbase_get_vexpress_cpu_clock_speed
	},

	{
		KBASE_CONFIG_ATTR_SECURE_BUT_LOSS_OF_PERFORMANCE,
		(uintptr_t)MALI_FALSE /* By default we prefer performance over security on r0p0-15dev0 and earlier */
	},

	{
		KBASE_CONFIG_ATTR_GPU_IRQ_THROTTLE_TIME_US,
		20
	},

	{
		KBASE_CONFIG_ATTR_END,
		0
	}
};

kbase_platform_config platform_config =
{
		.attributes                = config_attributes,
		.io_resources              = &io_resources,
		.midgard_type              = KBASE_MALI_T6F1
};


