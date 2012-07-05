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
 * @file mali_kbase_context.c
 * Base kernel context APIs
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_midg_regmap.h>

/**
 * @brief Create a kernel base context.
 *
 * Allocate and init a kernel base context. Calls
 * kbase_create_os_context() to setup OS specific structures.
 */
struct kbase_context *kbase_create_context(kbase_device *kbdev)
{
	struct kbase_context *kctx;
	osk_error osk_err;
	mali_error mali_err;

	OSK_ASSERT(kbdev != NULL);

	/* zero-inited as lot of code assume it's zero'ed out on create */
	kctx = osk_calloc(sizeof(*kctx));
	if (!kctx)
		goto out;

	kctx->kbdev = kbdev;
	kctx->as_nr = KBASEP_AS_NR_INVALID;

	if (kbase_mem_usage_init(&kctx->usage, kctx->kbdev->memdev.per_process_memory_limit >> OSK_PAGE_SHIFT))
	{
		goto free_kctx;
	}

	if (kbase_jd_init(kctx))
		goto free_memctx;

	mali_err = kbasep_js_kctx_init( kctx );
	if ( MALI_ERROR_NONE != mali_err )
	{
		goto free_jd; /* safe to call kbasep_js_kctx_term  in this case */
	}

	mali_err = kbase_event_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_jd;

	osk_err = osk_mutex_init(&kctx->reg_lock, OSK_LOCK_ORDER_MEM_REG);
	if (OSK_ERR_NONE != osk_err)
		goto free_event;

	/* Use a new *Shared Memory* allocator for GPU page tables.
	 * See MIDBASE-1534 for details. */
	osk_err = osk_phy_allocator_init(&kctx->pgd_allocator, 0, 0, NULL);
	if (OSK_ERR_NONE != osk_err)
		goto free_region_lock;

	mali_err = kbase_mmu_init(kctx);
	if(MALI_ERROR_NONE != mali_err)
		goto free_phy;

	kctx->pgd = kbase_mmu_alloc_pgd(kctx);
	if (!kctx->pgd)
		goto free_mmu;

	if (kbase_create_os_context(&kctx->osctx))
		goto free_pgd;

	kctx->nr_outstanding_atoms = 0;
	if ( OSK_ERR_NONE != osk_waitq_init(&kctx->complete_outstanding_waitq))
	{
		goto free_osctx;
	}
	osk_waitq_set(&kctx->complete_outstanding_waitq);

	/* Make sure page 0 is not used... */
	if(kbase_region_tracker_init(kctx))
		goto free_waitq;

	return kctx;

free_waitq:
	osk_waitq_term(&kctx->complete_outstanding_waitq);
free_osctx:
	kbase_destroy_os_context(&kctx->osctx);
free_pgd:
	kbase_mmu_free_pgd(kctx);
free_mmu:
	kbase_mmu_term(kctx);
free_phy:
	osk_phy_allocator_term(&kctx->pgd_allocator);
free_region_lock:
	osk_mutex_term(&kctx->reg_lock);
free_event:
	kbase_event_cleanup(kctx);
free_jd:
	/* Safe to call this one even when didn't initialize (assuming kctx was sufficiently zeroed) */
	kbasep_js_kctx_term(kctx);
	kbase_jd_exit(kctx);
free_memctx:
	kbase_mem_usage_term(&kctx->usage);
free_kctx:
	osk_free(kctx);
out:
	return NULL;

}
KBASE_EXPORT_SYMBOL(kbase_create_context)

/**
 * @brief Destroy a kernel base context.
 *
 * Destroy a kernel base context. Calls kbase_destroy_os_context() to
 * free OS specific structures. Will release all outstanding regions.
 */
void kbase_destroy_context(struct kbase_context *kctx)
{
	kbase_device *kbdev;

	OSK_ASSERT(NULL != kctx);

	kbdev = kctx->kbdev;
	OSK_ASSERT(NULL != kbdev);

	KBASE_TRACE_ADD( kbdev, CORE_CTX_DESTROY, kctx, NULL, 0u, 0u );

	/* Ensure the core is powered up for the destroy process */
	kbase_pm_context_active(kbdev);

	if(kbdev->hwcnt.kctx == kctx)
	{
		/* disable the use of the hw counters if the app didn't use the API correctly or crashed */
		KBASE_TRACE_ADD( kbdev, CORE_CTX_HWINSTR_TERM, kctx, NULL, 0u, 0u );
		OSK_PRINT_WARN(OSK_BASE_CTX,
					   "The privileged process asking for instrumentation forgot to disable it "
					   "before exiting. Will end instrumentation for them" );
		kbase_instr_hwcnt_disable(kctx);
	}

	kbase_jd_zap_context(kctx);
	kbase_event_cleanup(kctx);

	kbase_gpu_vm_lock(kctx);

	/* MMU is disabled as part of scheduling out the context */
	kbase_mmu_free_pgd(kctx);
	osk_phy_allocator_term(&kctx->pgd_allocator);
	kbase_region_tracker_term(kctx);
	kbase_destroy_os_context(&kctx->osctx);
	kbase_gpu_vm_unlock(kctx);

	/* Safe to call this one even when didn't initialize (assuming kctx was sufficiently zeroed) */
	kbasep_js_kctx_term(kctx);

	kbase_jd_exit(kctx);
	osk_mutex_term(&kctx->reg_lock);

	kbase_pm_context_idle(kbdev);

	kbase_mmu_term(kctx);

	kbase_mem_usage_term(&kctx->usage);

	osk_waitq_term(&kctx->complete_outstanding_waitq);
	osk_free(kctx);
}
KBASE_EXPORT_SYMBOL(kbase_destroy_context)

/**
 * Set creation flags on a context
 */
mali_error kbase_context_set_create_flags(kbase_context *kctx, u32 flags)
{
	mali_error err = MALI_ERROR_NONE;
	kbasep_js_kctx_info *js_kctx_info;
	OSK_ASSERT(NULL != kctx);

	js_kctx_info = &kctx->jctx.sched_info;

	/* Validate flags */
	if ( flags != (flags & BASE_CONTEXT_CREATE_KERNEL_FLAGS) )
	{
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out;
	}

	osk_mutex_lock( &js_kctx_info->ctx.jsctx_mutex );

	/* Ensure this is the first call */
	if ( (js_kctx_info->ctx.flags & KBASE_CTX_FLAG_CREATE_FLAGS_SET) != 0 )
	{
		OSK_PRINT_ERROR(OSK_BASE_CTX, "User attempted to set context creation flags more than once - not allowed");
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out_unlock;
	}

	js_kctx_info->ctx.flags |= KBASE_CTX_FLAG_CREATE_FLAGS_SET;

	/* Translate the flags */
	if ( (flags & BASE_CONTEXT_SYSTEM_MONITOR_SUBMIT_DISABLED) == 0 )
	{
		/* This flag remains set until it is explicitly cleared */
		js_kctx_info->ctx.flags &= ~((u32)KBASE_CTX_FLAG_SUBMIT_DISABLED);
	}

	if ( (flags & BASE_CONTEXT_HINT_ONLY_COMPUTE) != 0 )
	{
		js_kctx_info->ctx.flags |= (u32)KBASE_CTX_FLAG_HINT_ONLY_COMPUTE;
	}

	/* Latch the initial attributes into the Job Scheduler */
	kbasep_js_ctx_attr_set_initial_attrs( kctx->kbdev, kctx );


out_unlock:
	osk_mutex_unlock( &js_kctx_info->ctx.jsctx_mutex );
out:
	return err;
}
KBASE_EXPORT_SYMBOL(kbase_context_set_create_flags)
