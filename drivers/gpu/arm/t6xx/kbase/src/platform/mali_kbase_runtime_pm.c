/* drivers/gpu/t6xx/kbase/src/platform/mali_kbase_runtime_pm.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 runtime pm driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_runtime_pm.c
 * Runtime PM
 */

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_uku.h>
#include <kbase/src/common/mali_kbase_mem.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/linux/mali_kbase_mem_linux.h>
#include <kbase/mali_ukk.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <kbase/src/platform/mali_kbase_platform.h>
#include <kbase/src/platform/mali_kbase_runtime_pm.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <kbase/src/common/mali_kbase_gator.h>

#define RUNTIME_PM_DELAY_TIME 100
static void kbase_device_runtime_workqueue_callback(struct work_struct *work)
{
	int result;
	struct kbase_device *kbdev;

	kbdev = container_of(work, struct kbase_device, runtime_pm_workqueue.work);
	/********************************************
	 *
	 *  This is workaround about occurred kernel panic when you turn off the system.
	 *
	 *  System kernel will call the "__pm_runtime_disable" when you turn off the system.
	 *  After that function, System kernel do not run the runtimePM API more.
	 *
	 *  So, this code is check the "dev->power.disable_depth" value is not zero.
	 *
	********************************************/
	if(kbdev->osdev.dev->power.disable_depth > 0)
		return;

	result = pm_runtime_suspend(kbdev->osdev.dev);
	kbase_platform_clock_off(kbdev);

#if MALI_GATOR_SUPPORT
	kbase_trace_mali_timeline_event(GATOR_MAKE_EVENT(ACTIVITY_RTPM_CHANGED, ACTIVITY_RTPM));
#endif
#if MALI_RTPM_DEBUG
	printk( "kbase_device_runtime_workqueue_callback, usage_count=%d\n", atomic_read(&kbdev->osdev.dev->power.usage_count));
#endif

	if(result < 0 && result != -EAGAIN)
		OSK_PRINT_ERROR(OSK_BASE_PM, "pm_runtime_put_sync failed (%d)\n", result);
}
void kbase_device_runtime_init_workqueue(struct device *dev)
{
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);
	INIT_DELAYED_WORK(&kbdev->runtime_pm_workqueue, kbase_device_runtime_workqueue_callback);
}

/** Disable runtime pm
 *
 * @param dev	The device to enable rpm
 *
 * @return A standard Linux error code
 */
void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->osdev.dev);
}

/** Initialize runtiem pm fields in given device
 *
 * @param dev	The device to initialize
 *
 * @return A standard Linux error code
 */

mali_error kbase_device_runtime_init(struct kbase_device *kbdev)
{
	pm_suspend_ignore_children(kbdev->osdev.dev, true);
	pm_runtime_enable(kbdev->osdev.dev);
	kbase_device_runtime_init_workqueue(kbdev->osdev.dev);
	return MALI_ERROR_NONE;
}

void kbase_device_runtime_get_sync(struct device *dev)
{
	int result;
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	kbdev = dev_get_drvdata(dev);

	platform = (struct exynos_context *) kbdev->platform_context;
	if(!platform)
		return;

	/********************************************
	 *
	 *  This is workaround about occurred kernel panic when you turn off the system.
	 *
	 *  System kernel will call the "__pm_runtime_disable" when you turn off the system.
	 *  After that function, System kernel do not run the runtimePM API more.
	 *
	 *  So, this code is check the "dev->power.disable_depth" value is not zero.
	 *
	********************************************/
	if(dev->power.disable_depth > 0) {
		if(platform->cmu_pmu_status == 0)
			kbase_platform_cmu_pmu_control(kbdev, 1);
		return;
	}

	if(delayed_work_pending(&kbdev->runtime_pm_workqueue)) {
		cancel_delayed_work_sync(&kbdev->runtime_pm_workqueue);
	}

	kbase_platform_clock_on(kbdev);
	pm_runtime_get_noresume(dev);
	result = pm_runtime_resume(dev);

#if MALI_GATOR_SUPPORT
	kbase_trace_mali_timeline_event(GATOR_MAKE_EVENT(ACTIVITY_RTPM_CHANGED, ACTIVITY_RTPM) | 1);
#endif
#if MALI_RTPM_DEBUG
	//printk( "kbase_device_runtime_get_sync, usage_count=%d, runtime_status=%d\n", atomic_read(&dev->power.usage_count), dev->power.runtime_status);
	printk( "+++kbase_device_runtime_get_sync, usage_count=%d\n", atomic_read(&dev->power.usage_count));
#endif

	/********************************************
	 *
	 *  This check is re-confirm about maybe context switch by another cpu when turn off the system.
	 *
	 *  runtimePM put_sync -------- runtimePM get_sync -------- runtimePM put_sync          : CPU 0
	 *                                      \
	 *                                       \ ( context running by another core. )
	 *                                        \
	 *                                         - (turn off the system) runtimePM disable    : CPU 1
	 *                                                                    \
	 *                                                                     \
	 *                                                                      => do not success implement runtimePM API
	********************************************/
	if(result < 0 && result == -EAGAIN)
		kbase_platform_cmu_pmu_control(kbdev, 1);
	else if(result < 0)
		OSK_PRINT_ERROR(OSK_BASE_PM, "pm_runtime_get_sync failed (%d)\n", result);
}

void kbase_device_runtime_put_sync(struct device *dev)
{
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	if(delayed_work_pending(&kbdev->runtime_pm_workqueue)) {
		cancel_delayed_work_sync(&kbdev->runtime_pm_workqueue);
	}

	pm_runtime_put_noidle(kbdev->osdev.dev);
	schedule_delayed_work(&kbdev->runtime_pm_workqueue, RUNTIME_PM_DELAY_TIME/(1000/HZ));
#if MALI_RTPM_DEBUG
	printk( "---kbase_device_runtime_put_sync, usage_count=%d\n", atomic_read(&kbdev->osdev.dev->power.usage_count));
#endif
}
