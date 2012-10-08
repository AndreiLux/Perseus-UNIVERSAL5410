/*
 *
 * (C) COPYRIGHT 2010-2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



/**
 * @file mali_kbase_js_affinity.c
 * Base kernel affinity manager APIs
 */

#include <kbase/src/common/mali_kbase.h>
#include "mali_kbase_js_affinity.h"

#if MALI_DEBUG && 0 /* disabled to avoid compilation warnings */

STATIC void debug_get_binary_string(const u64 n, char *buff, const int size)
{
	unsigned int i;
	for (i = 0; i < size; i++)
	{
		buff[i] = ((n >> i) & 1) ? '*' : '-';
	}
	buff[size] = '\0';
}

#define N_CORES 8
STATIC void debug_print_affinity_info(const kbase_device *kbdev, const kbase_jd_atom *katom, int js, u64 affinity)
{
	char buff[N_CORES +1];
	char buff2[N_CORES +1];
	base_jd_core_req core_req = katom->atom->core_req;
	u8 nr_nss_ctxs_running =  kbdev->js_data.runpool_irq.ctx_attr_ref_count[KBASEP_JS_CTX_ATTR_NSS];
	u64 shader_present_bitmap = kbdev->shader_present_bitmap;

	debug_get_binary_string(shader_present_bitmap, buff, N_CORES);
	debug_get_binary_string(affinity, buff2, N_CORES);

	OSK_PRINT_INFO(OSK_BASE_JM, "Job: NSS COH FS  CS   T  CF   V  JS | NSS_ctx | GPU:12345678 | AFF:12345678");
	OSK_PRINT_INFO(OSK_BASE_JM, "      %s   %s   %s   %s   %s   %s   %s   %u |    %u    |     %s |     %s",
				   core_req & BASE_JD_REQ_NSS            ? "*" : "-",
				   core_req & BASE_JD_REQ_COHERENT_GROUP ? "*" : "-",
				   core_req & BASE_JD_REQ_FS             ? "*" : "-",
				   core_req & BASE_JD_REQ_CS             ? "*" : "-",
				   core_req & BASE_JD_REQ_T              ? "*" : "-",
				   core_req & BASE_JD_REQ_CF             ? "*" : "-",
				   core_req & BASE_JD_REQ_V              ? "*" : "-",
				   js, nr_nss_ctxs_running, buff, buff2);
}

#endif /* MALI_DEBUG */

OSK_STATIC_INLINE mali_bool affinity_job_uses_high_cores( kbase_jd_atom *katom )
{
#if BASE_HW_ISSUE_8987 != 0
	kbase_context *kctx;
	kbase_context_flags ctx_flags;

	kctx = katom->kctx;
	ctx_flags = kctx->jctx.sched_info.ctx.flags;

	/* In this HW Workaround, compute-only jobs/contexts use the high cores
	 * during a core-split, all other contexts use the low cores. */
	return (mali_bool)((katom->core_req & BASE_JD_REQ_ONLY_COMPUTE) != 0
					   || (ctx_flags & KBASE_CTX_FLAG_HINT_ONLY_COMPUTE) != 0);

#else /* BASE_HW_ISSUE_8987 != 0 */
	base_jd_core_req core_req = katom->core_req;
	/* NSS-ness determines whether the high cores in a core split are used */
	return (mali_bool)(core_req & BASE_JD_REQ_NSS);

#endif /* BASE_HW_ISSUE_8987 != 0 */
}

/*
 * As long as it has been decided to have a deeper modification of
 * what job scheduler, power manager and affinity manager will
 * implement, this function is just an intermediate step that
 * assumes:
 * - all working cores will be powered on when this is called.
 * - largest current configuration is a T658 (2x4 cores).
 * - It has been decided not to have hardcoded values so the low
 *   and high cores in a core split will be evently distributed.
 * - Odd combinations of core requirements have been filtered out
 *   and do not get to this function (e.g. CS+T+NSS is not
 *   supported here).
 * - This function is frequently called and can be optimized,
 *   (see notes in loops), but as the functionallity will likely
 *   be modified, optimization has not been addressed.
*/
void kbase_js_choose_affinity(u64 *affinity, kbase_device *kbdev, kbase_jd_atom *katom, int js)
{
	base_jd_core_req core_req = katom->core_req;
	u64 shader_present_bitmap = kbdev->shader_present_bitmap;
	CSTD_UNUSED(js);

	OSK_ASSERT(0 != shader_present_bitmap);

	if (1 == kbdev->gpu_props.num_cores)
	{
		/* trivial case only one core, nothing to do */
		*affinity = shader_present_bitmap;
	}
	else if ( kbase_affinity_requires_split( kbdev ) == MALI_FALSE )
	{
		/* All cores are available when no core split is required */
		if (core_req & (BASE_JD_REQ_COHERENT_GROUP))
		{
			*affinity = kbdev->gpu_props.props.coherency_info.group[0].core_mask;
		}
		else
		{
			*affinity = shader_present_bitmap;
		}
	}
	else
	{
		/* Core split required - divide cores in two non-overlapping groups */
		u64 low_bitmap, high_bitmap;
		int n_high_cores = kbdev->gpu_props.num_cores >> 1;
		OSK_ASSERT(0 != n_high_cores);

		/* compute the reserved high cores bitmap */
		high_bitmap = ~0;
		/* note: this can take a while, optimization desirable */
		while (n_high_cores != osk_count_set_bits(high_bitmap & shader_present_bitmap))
		{
			high_bitmap = high_bitmap << 1;
		}
		high_bitmap &= shader_present_bitmap;

		/* now decide 4 different situations depending on the low or high
		 * set of cores and requiring coherent group or not */
		if (affinity_job_uses_high_cores( katom ))
		{
			unsigned int num_core_groups = kbdev->gpu_props.num_core_groups;
			OSK_ASSERT(0 != num_core_groups);

			if ((core_req & BASE_JD_REQ_COHERENT_GROUP) && (1 != num_core_groups))
			{
				/* high set of cores requiring coherency and coherency matters
				 * because we got more than one core group */
				u64 group1_mask = kbdev->gpu_props.props.coherency_info.group[1].core_mask;
				*affinity = high_bitmap & group1_mask;
			}
			else
			{
				/* high set of cores not requiring coherency or coherency is
				   assured as we only have one core_group */
				*affinity = high_bitmap;
			}
		}
		else
		{
			low_bitmap = shader_present_bitmap ^ high_bitmap;
		
			if (core_req & BASE_JD_REQ_COHERENT_GROUP)
			{
				/* low set of cores and req coherent group */
				u64 group0_mask = kbdev->gpu_props.props.coherency_info.group[0].core_mask;
				u64 low_coh_bitmap = low_bitmap & group0_mask;
				*affinity = low_coh_bitmap;
			}
			else
			{
				/* low set of cores and does not req coherent group */
				*affinity = low_bitmap;
			}
		}
	}

	OSK_ASSERT(*affinity != 0);
}

OSK_STATIC_INLINE mali_bool kbase_js_affinity_is_violating( u64 *affinities )
{
	/* This implementation checks whether:
	 * - the two slots involved in Generic thread creation have intersecting affinity
	 * - Cores for the fragment slot (slot 0) would compete with cores for slot 2.
	 *  - This is due to micro-architectural issues where a job in slot A targetting
	 * cores used by slot B could prevent the job in slot B from making progress
	 * until the job in slot A has completed.
	 *  - In our case, slot 2 is used for batch/NSS jobs, so if their affinity
	 * intersected with slot 0, then fragment jobs would be delayed by the batch/NSS
	 * jobs.
	 *
	 * @note It just so happens that these restrictions also allow
	 * BASE_HW_ISSUE_8987 to be worked around by placing on job slot 2 the
	 * atoms from ctxs with KBASE_CTX_FLAG_HINT_ONLY_COMPUTE flag set
	 */
	u64 affinity_set_left;
	u64 affinity_set_right;
	u64 intersection;
	OSK_ASSERT( affinities != NULL );

	affinity_set_left = affinities[0] | affinities[1];
	affinity_set_right = affinities[2];

	/* A violation occurs when any bit in the left_set is also in the right_set */
	intersection = affinity_set_left & affinity_set_right;

	return (mali_bool)( intersection != (u64)0u );
}

mali_bool kbase_js_affinity_would_violate( kbase_device *kbdev, int js, u64 affinity )
{
	kbasep_js_device_data *js_devdata;
	u64 new_affinities[BASE_JM_MAX_NR_SLOTS];

	OSK_ASSERT( kbdev != NULL );
	OSK_ASSERT( js <  BASE_JM_MAX_NR_SLOTS );
	js_devdata = &kbdev->js_data;

	OSK_MEMCPY( new_affinities, js_devdata->runpool_irq.slot_affinities, sizeof(js_devdata->runpool_irq.slot_affinities) );

	new_affinities[ js ] |= affinity;

	return kbase_js_affinity_is_violating( new_affinities );
}

void kbase_js_affinity_retain_slot_cores( kbase_device *kbdev, int js, u64 affinity )
{
	kbasep_js_device_data *js_devdata;
	u64 cores;

	OSK_ASSERT( kbdev != NULL );
	OSK_ASSERT( js <  BASE_JM_MAX_NR_SLOTS );
	js_devdata = &kbdev->js_data;

	OSK_ASSERT( kbase_js_affinity_would_violate( kbdev, js, affinity ) == MALI_FALSE );

	cores = affinity;
	while (cores)
	{
		int bitnum = 63 - osk_clz_64(cores);
		u64 bit = 1ULL << bitnum;
		s8 cnt;

		OSK_ASSERT( js_devdata->runpool_irq.slot_affinity_refcount[ js ][bitnum] < BASE_JM_SUBMIT_SLOTS );

		cnt = ++(js_devdata->runpool_irq.slot_affinity_refcount[ js ][bitnum]);

		if ( cnt == 1 )
		{
			js_devdata->runpool_irq.slot_affinities[js] |= bit;
		}

		cores &= ~bit;
	}

}

void kbase_js_affinity_release_slot_cores( kbase_device *kbdev, int js, u64 affinity )
{
	kbasep_js_device_data *js_devdata;
	u64 cores;

	OSK_ASSERT( kbdev != NULL );
	OSK_ASSERT( js < BASE_JM_MAX_NR_SLOTS );
	js_devdata = &kbdev->js_data;

	cores = affinity;
	while (cores)
	{
		int bitnum = 63 - osk_clz_64(cores);
		u64 bit = 1ULL << bitnum;
		s8 cnt;

		OSK_ASSERT( js_devdata->runpool_irq.slot_affinity_refcount[ js ][bitnum] > 0 );

		cnt = --(js_devdata->runpool_irq.slot_affinity_refcount[ js ][bitnum]);

		if (0 == cnt)
		{
			js_devdata->runpool_irq.slot_affinities[js] &= ~bit;
		}

		cores &= ~bit;
	}

}

void kbase_js_affinity_slot_blocked_an_atom( kbase_device *kbdev, int js )
{
	kbasep_js_device_data *js_devdata;

	OSK_ASSERT( kbdev != NULL );
	OSK_ASSERT( js < BASE_JM_MAX_NR_SLOTS );
	js_devdata = &kbdev->js_data;

	js_devdata->runpool_irq.slots_blocked_on_affinity |= 1u << js;
}

void kbase_js_affinity_submit_to_blocked_slots( kbase_device *kbdev )
{
	kbasep_js_device_data *js_devdata;
	u16 slots;

	OSK_ASSERT( kbdev != NULL );
	js_devdata = &kbdev->js_data;

	osk_mutex_lock( &js_devdata->runpool_mutex );

	/* Must take a copy because submitting jobs will update this member
	 * We don't take a lock here - a data barrier was issued beforehand */
	slots = js_devdata->runpool_irq.slots_blocked_on_affinity;
	while (slots)
	{
		int bitnum = 31 - osk_clz(slots);
		u16 bit = 1u << bitnum;
		slots &= ~bit;

		KBASE_TRACE_ADD_SLOT( kbdev, JS_AFFINITY_SUBMIT_TO_BLOCKED, NULL, NULL, 0u, bitnum);

		osk_spinlock_irq_lock( &js_devdata->runpool_irq.lock );
		/* must update this before we submit, incase it's set again */
		js_devdata->runpool_irq.slots_blocked_on_affinity &= ~bit;
		osk_spinlock_irq_unlock( &js_devdata->runpool_irq.lock );

		kbasep_js_try_run_next_job_on_slot( kbdev, bitnum );

		/* Don't re-read slots_blocked_on_affinity after this - it could loop for a long time */
	}
	osk_mutex_unlock( &js_devdata->runpool_mutex );

}

#if MALI_DEBUG != 0
void kbase_js_debug_log_current_affinities( kbase_device *kbdev )
{
	kbasep_js_device_data *js_devdata;
	int slot_nr;

	OSK_ASSERT( kbdev != NULL );
	js_devdata = &kbdev->js_data;

	for ( slot_nr = 0; slot_nr < 3 ; ++slot_nr )
	{
		KBASE_TRACE_ADD_SLOT_INFO( kbdev, JS_AFFINITY_CURRENT, NULL, NULL, 0u, slot_nr, (u32)js_devdata->runpool_irq.slot_affinities[slot_nr] );
	}
}
#endif /* MALI_DEBUG != 0 */
