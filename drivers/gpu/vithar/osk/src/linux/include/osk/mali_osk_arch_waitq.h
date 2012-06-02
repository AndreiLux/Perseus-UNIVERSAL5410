/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_ARCH_WAITQ_H
#define _OSK_ARCH_WAITQ_H

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

#include <linux/wait.h>
#include <linux/sched.h>

/*
 * Note:
 *
 * We do not need locking on the signalled member (see its doxygen description)
 */

OSK_STATIC_INLINE osk_error osk_waitq_init(osk_waitq * const waitq)
{
	OSK_ASSERT(NULL != waitq);
	waitq->signaled = MALI_FALSE;
	init_waitqueue_head(&waitq->wq);
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE void osk_waitq_wait(osk_waitq *waitq)
{
	OSK_ASSERT(NULL != waitq);
	wait_event(waitq->wq, waitq->signaled != MALI_FALSE);
}

OSK_STATIC_INLINE void osk_waitq_set(osk_waitq *waitq)
{
	OSK_ASSERT(NULL != waitq);
	waitq->signaled = MALI_TRUE;
	wake_up(&waitq->wq);
}

OSK_STATIC_INLINE void osk_waitq_clear(osk_waitq *waitq)
{
	OSK_ASSERT(NULL != waitq);
	waitq->signaled = MALI_FALSE;
}

OSK_STATIC_INLINE void osk_waitq_term(osk_waitq *waitq)
{
	OSK_ASSERT(NULL != waitq);
	/* NOP on Linux */
}

#endif /* _OSK_ARCH_WAITQ_H_ */
