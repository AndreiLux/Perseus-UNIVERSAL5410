/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_linux.h
 * Base kernel APIs, Linux implementation.
 */

#ifndef _KBASE_LINUX_H_
#define _KBASE_LINUX_H_

/* All things that are needed for the Linux port. */
#if MALI_LICENSE_IS_GPL
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#endif
#include <linux/list.h>
#include <linux/module.h>

typedef struct kbase_os_context
{
	u64			cookies;
	osk_dlist		reg_pending;
	wait_queue_head_t	event_queue;
	pid_t tgid;
} kbase_os_context;


#define DEVNAME_SIZE	16

typedef struct kbase_os_device
{
#if MALI_LICENSE_IS_GPL
	struct list_head	entry;
	struct device		*dev;
	struct miscdevice	mdev;
#else
	struct cdev		*dev;
#endif
	u64					reg_start;
	size_t				reg_size;
	void __iomem		*reg;
	struct resource		*reg_res;
	struct {
		int		irq;
		int		flags;
	} irqs[3];
	char			devname[DEVNAME_SIZE];

#if MALI_NO_MALI
	void *model;
	struct kmem_cache *irq_slab;
	osk_workq irq_workq;
	osk_atomic serving_job_irq;
	osk_atomic serving_gpu_irq;
	osk_atomic serving_mmu_irq;
	osk_spinlock_irq reg_op_lock;
#endif
} kbase_os_device;

#define KBASE_OS_SUPPORT	1

#if defined(MALI_KERNEL_TEST_API)
#if (1 == MALI_KERNEL_TEST_API)
#define KBASE_EXPORT_TEST_API(func)		EXPORT_SYMBOL(func);
#else
#define KBASE_EXPORT_TEST_API(func)
#endif
#else
#define KBASE_EXPORT_TEST_API(func)
#endif

#define KBASE_EXPORT_SYMBOL(func)		EXPORT_SYMBOL(func);

#endif /* _KBASE_LINUX_H_ */
