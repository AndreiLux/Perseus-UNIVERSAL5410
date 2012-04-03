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



/**
 * @file mali_kbase_js_affinity.h
 * Affinity Manager internal APIs.
 */

#ifndef _KBASE_JS_AFFINITY_H_
#define _KBASE_JS_AFFINITY_H_



/**
 * @addtogroup base_api
 * @{
 */

/**
 * @addtogroup base_kbase_api
 * @{
 */


/**
 * @addtogroup kbase_js_affinity Affinity Manager internal APIs.
 * @{
 *
 */


/**
 * @brief Compute affinity for a given job.
 *
 * Currently assumes an all-on/all-off power management policy.
 * Also assumes there is at least one core with tiler available.
 * Will try to produce an even distribution of cores for SS and
 * NSS jobs. SS jobs will be given cores starting from core-group
 * 0 forward to n. NSS jobs will be given cores from core-group n
 * backwards to 0. This way for example in a T658 SS jobs will
 * tend to run on cores from core-group 0 and NSS jobs will tend
 * to run on cores from core-group 1.
 * An assertion will be raised if computed affinity is 0
 *
 * @param[out] affinity Affinity bitmap computed
 * @param kbdev The kbase device structure of the device
 * @param katom Job chain of which affinity is going to be found
 * @param js    Slot the job chain is being submitted

 */
void kbase_js_choose_affinity( u64 *affinity, kbase_device *kbdev, kbase_jd_atom *katom, int js );

/**
 * @brief Determine whether a proposed \a affinity on job slot \a js would
 * cause a violation of affinity restrictions.
 *
 * The following locks must be held by the caller:
 * - kbasep_js_device_data::runpool_irq::lock
 */
mali_bool kbase_js_affinity_would_violate( kbase_device *kbdev, int js, u64 affinity );

/**
 * @brief Affinity tracking: retain cores used by a slot
 *
 * The following locks must be held by the caller:
 * - kbasep_js_device_data::runpool_irq::lock
 */
void kbase_js_affinity_retain_slot_cores( kbase_device *kbdev, int js, u64 affinity );

/**
 * @brief Affinity tracking: release cores used by a slot
 *
 * Cores \b must be released as soon as a job is dequeued from a slot's 'submit
 * slots', and before another job is submitted to those slots. Otherwise, the
 * refcount could exceed the maximum number submittable to a slot,
 * BASE_JM_SUBMIT_SLOTS.
 *
 * The following locks must be held by the caller:
 * - kbasep_js_device_data::runpool_irq::lock
 */
void kbase_js_affinity_release_slot_cores( kbase_device *kbdev, int js, u64 affinity );

/**
 * @brief Register a slot as blocking atoms due to affinity violations
 *
 * Once a slot has been registered, we must check after every atom completion
 * (including those on different slots) to see if the slot can be
 * unblocked. This is done by calling
 * kbase_js_affinity_submit_to_blocked_slots(), which will also deregister the
 * slot if it no long blocks atoms due to affinity violations.
 *
 * The following locks must be held by the caller:
 * - kbasep_js_device_data::runpool_irq::lock
 */
void kbase_js_affinity_slot_blocked_an_atom( kbase_device *kbdev, int js );

/**
 * @brief Submit to job slots that have registered that an atom was blocked on
 * the slot previously due to affinity violations.
 *
 * This submits to all slots registered by
 * kbase_js_affinity_slot_blocked_an_atom(). If submission succeeded, then the
 * slot is deregistered as having blocked atoms due to affinity
 * violations. Otherwise it stays registered, and the next atom to complete
 * must attempt to submit to the blocked slots again.
 *
 * The following locking conditions are made on the caller:
 * - it must \em not hold kbasep_js_device_data::runpool_mutex (as this will be
 * obtained internally)
 * - it must \em not hold kbdev->jm_slots[ \a js ].lock (as this will be
 * obtained internally)
 * - it must \em not hold kbasep_js_device_data::runpool_irq::lock, (as this will be
 * obtained internally)
 */
void kbase_js_affinity_submit_to_blocked_slots( kbase_device *kbdev );

/**
 * @brief Output to the Trace log the current tracked affinities on all slots
 */
#if MALI_DEBUG != 0
void kbase_js_debug_log_current_affinities( kbase_device *kbdev );
#else /*  MALI_DEBUG != 0 */
OSK_STATIC_INLINE void kbase_js_debug_log_current_affinities( kbase_device *kbdev )
{
}
#endif /*  MALI_DEBUG != 0 */

/**
 * @brief Decide whether a split in core affinity is required across job slots
 *
 * The following locking conditions are made on the caller:
 * - it must hold kbasep_js_device_data::runpool_irq::lock
 *
 * @param kbdev The kbase device structure of the device
 * @return MALI_FALSE if a core split is not required
 * @return != MALI_FALSE if a core split is required.
 */
OSK_STATIC_INLINE mali_bool kbase_affinity_requires_split(kbase_device *kbdev)
{
	kbasep_js_device_data *js_devdata;

	OSK_ASSERT( kbdev != NULL );
	js_devdata = &kbdev->js_data;
#if BASE_HW_ISSUE_8987 != 0
	/* In this case, a mix of Compute+Non-Compute determines whether a
	 * core-split is required, to ensure jobs with different numbers of RMUs
	 * don't use the same cores.
	 *
	 * When it's entirely compute, or entirely non-compute, then no split is
	 * required.
	 *
	 * A context can be both Compute and Non-compute, in which case this will
	 * correctly decide that a core-split is required. */
	{
		s8 nr_compute_ctxs = kbasep_js_ctx_attr_count_on_runpool( kbdev, KBASEP_JS_CTX_ATTR_COMPUTE );
		s8 nr_noncompute_ctxs = kbasep_js_ctx_attr_count_on_runpool( kbdev, KBASEP_JS_CTX_ATTR_NON_COMPUTE );

		return (mali_bool)( nr_compute_ctxs > 0 && nr_noncompute_ctxs > 0 );
	}
#else /* BASE_HW_ISSUE_8987 != 0 */
	/* NSS/SS state determines whether a core-split is required */
	return (mali_bool)( js_devdata->runpool_irq.ctx_attr_ref_count[KBASEP_JS_CTX_ATTR_NSS] != 0 );
#endif /* BASE_HW_ISSUE_8987 != 0 */
}


/**
 * @brief Decide whether it is possible to submit a job to a particular job slot in the current status
 *
 * Will check if submitting to the given job slot is allowed in the current status.
 * For example using job slot 2 while in soft-stoppable state is not allowed by the
 * policy. This function should be called prior to submitting a job to a slot to
 * make sure policy rules are not violated.
 *
 * The following locking conditions are made on the caller:
 * - it must hold kbasep_js_device_data::runpool_irq::lock
 *
 * @param js_devdata Device info, will be read to obtain current state
 * @param js         Job slot number to check for allowance
 */
static INLINE mali_bool kbase_js_can_run_job_on_slot_no_lock( kbasep_js_device_data *js_devdata, int js )
{
	/* Submitting to job slot 2 only when have contexts containing some atoms that will only be queued on slot 2 */
#if BASE_HW_ISSUE_8987 != 0
	/* For this workaround, only compute-only contexts put atoms on slot 2 */
	return (mali_bool)(js != 2 || js_devdata->runpool_irq.ctx_attr_ref_count[KBASEP_JS_CTX_ATTR_COMPUTE] != 0);

#else /* BASE_HW_ISSUE_8987 != 0 */
	/* Normally, only contexts with NSS jobs are those that put atoms on slot 2 */

	return (mali_bool)(js != 2 || js_devdata->runpool_irq.ctx_attr_ref_count[KBASEP_JS_CTX_ATTR_NSS] != 0);
#endif /* BASE_HW_ISSUE_8987 != 0 */
}


/** @} */ /* end group kbase_js_affinity */
/** @} */ /* end group base_kbase_api */
/** @} */ /* end group base_api */





#endif /* _KBASE_JS_AFFINITY_H_ */
