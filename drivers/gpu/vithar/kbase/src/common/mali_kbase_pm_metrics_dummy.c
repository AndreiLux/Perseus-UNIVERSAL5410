/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_pm_metrics_dummy.c
 * Dummy Metrics for power management.
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_pm.h>

void kbase_pm_register_vsync_callback(kbase_device *kbdev)
{
	OSK_ASSERT(kbdev != NULL);

	/* no VSync metrics will be available */
	kbdev->pm.metrics.platform_data = NULL;
}

void kbase_pm_unregister_vsync_callback(kbase_device *kbdev)
{
	OSK_ASSERT(kbdev != NULL);
}
