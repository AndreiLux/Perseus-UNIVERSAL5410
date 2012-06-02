/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_ARCH_CREDENTIALS_H_
#define _OSK_ARCH_CREDENTIALS_H_

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

#include <linux/cred.h>

OSK_STATIC_INLINE mali_bool osk_is_privileged(void)
{
	mali_bool is_privileged = MALI_FALSE;

	/* Check if the caller is root */
	if (current_euid() == 0)
	{
		is_privileged = MALI_TRUE;
	}

	return is_privileged;
}

OSK_STATIC_INLINE mali_bool osk_is_policy_realtime(void)
{
	int policy = current->policy;

	if (policy == SCHED_FIFO || policy == SCHED_RR)
	{
		return MALI_TRUE;
	}

	return MALI_FALSE;
}

OSK_STATIC_INLINE void osk_get_process_priority(osk_process_priority *prio)
{
	/* Note that we return the current process priority.
	 * If called from a kernel thread the priority returned
	 * will be the kernel thread priority and not the user
	 * process that is currently submitting jobs to the scheduler.
	 */
	OSK_ASSERT(prio);

	if(osk_is_policy_realtime())
	{
		prio->is_realtime = MALI_TRUE;
		/* NOTE: realtime range was in the range 0..99 (lowest to highest) so we invert
		 * the priority and scale to -20..0 to normalize the result with the NICE range
		 */
		prio->priority = (((MAX_RT_PRIO-1) - current->rt_priority) / 5) - 20;
		/* Realtime range returned:
		 * -20 - highest priority
		 *  0  - lowest priority
		 */
	}
	else
	{
		prio->is_realtime = MALI_FALSE;
		prio->priority = (current->static_prio - MAX_RT_PRIO) - 20;
		/* NICE range returned:
		 * -20 - highest priority
		 * +19 - lowest priority
		 */
	}
}

#endif /* _OSK_ARCH_CREDENTIALS_H_ */
