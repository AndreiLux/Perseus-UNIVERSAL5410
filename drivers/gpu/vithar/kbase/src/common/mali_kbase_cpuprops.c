/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_cpuprops.c
 * Base kernel property query APIs
 */

#include "mali_kbase_cpuprops.h"
#include "mali_kbase.h"
#include "mali_kbase_uku.h"
#include <kbase/mali_kbase_config.h>
#include <osk/mali_osk.h>

int kbase_cpuprops_get_default_clock_speed(u32 *clock_speed)
{
	OSK_ASSERT( NULL != clock_speed );

	*clock_speed = 100;
	return 0;
}

mali_error kbase_cpuprops_uk_get_props(struct kbase_context *kctx, kbase_uk_cpuprops * kbase_props)
{
	int result;
	kbase_cpuprops_clock_speed_function kbase_cpuprops_uk_get_clock_speed;

	kbase_props->props.cpu_l1_dcache_line_size_log2 = OSK_L1_DCACHE_LINE_SIZE_LOG2;
	kbase_props->props.cpu_l1_dcache_size           = OSK_L1_DCACHE_SIZE;
	kbase_props->props.cpu_flags                    = BASE_CPU_PROPERTY_FLAG_LITTLE_ENDIAN;

	kbase_props->props.nr_cores = OSK_NUM_CPUS;
	kbase_props->props.cpu_page_size_log2 = OSK_PAGE_SHIFT;
	kbase_props->props.available_memory_size = OSK_MEM_PAGES << OSK_PAGE_SHIFT;

	kbase_cpuprops_uk_get_clock_speed = (kbase_cpuprops_clock_speed_function)kbasep_get_config_value( kctx->kbdev, kctx->kbdev->config_attributes, KBASE_CONFIG_ATTR_CPU_SPEED_FUNC );
	result = kbase_cpuprops_uk_get_clock_speed(&kbase_props->props.max_cpu_clock_speed_mhz);
	if (result != 0)
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}

	return MALI_ERROR_NONE;
}
