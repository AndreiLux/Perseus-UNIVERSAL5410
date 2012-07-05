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
 * @file mali_osk_arch_timers_commercial.h
 * Implementation of the OS abstraction layer for the kernel device driver
 * For Commercial builds - does not use the hrtimers functionality.
 */

#ifndef _OSK_ARCH_TIMERS_COMMERCIAL_H
#define _OSK_ARCH_TIMERS_COMMERCIAL_H

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

OSK_STATIC_INLINE osk_error osk_timer_init(osk_timer * const tim)
{
	OSK_ASSERT(NULL != tim);
	init_timer(&tim->timer);
	OSK_DEBUG_CODE(	tim->active = MALI_FALSE );
	OSK_ASSERT(0 ==	object_is_on_stack(tim));
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_on_stack_init(osk_timer * const tim)
{
	OSK_ASSERT(NULL != tim);
	init_timer_on_stack(&tim->timer);
	OSK_DEBUG_CODE(	tim->active = MALI_FALSE );
	OSK_ASSERT(0 !=	object_is_on_stack(tim));
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_start(osk_timer *tim, u32 delay)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != tim->timer.function);
	OSK_ASSERT(0 != delay);
	tim->timer.expires = jiffies + ((delay * HZ + 999) / 1000);
	add_timer(&tim->timer);
	OSK_DEBUG_CODE(	tim->active = MALI_TRUE );
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_start_ns(osk_timer *tim, u64 delay)
{
	osk_error err;
	osk_divmod6432(&delay, 1000000);

	err = osk_timer_start( tim, delay );

	return err;
}

OSK_STATIC_INLINE osk_error osk_timer_modify(osk_timer *tim, u32 delay)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != tim->timer.function);
	OSK_ASSERT(0 != delay);
	mod_timer(&tim->timer, jiffies + ((delay * HZ + 999) / 1000));
	OSK_DEBUG_CODE(	tim->active = MALI_TRUE );
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_modify_ns(osk_timer *tim, u64 new_delay)
{
	osk_error err;
	osk_divmod6432(&new_delay, 1000000);

	err = osk_timer_modify( tim, new_delay );
	return err;
}

OSK_STATIC_INLINE void osk_timer_stop(osk_timer *tim)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != tim->timer.function);
	del_timer_sync(&tim->timer);
	OSK_DEBUG_CODE( tim->active = MALI_FALSE );
}

OSK_STATIC_INLINE void osk_timer_callback_set(osk_timer *tim, osk_timer_callback callback, void *data)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != callback);
	OSK_DEBUG_CODE(
		if (MALI_FALSE == tim->active)
		{
		}
	);
	/* osk_timer_callback uses void * for the callback parameter instead of unsigned long in Linux */
	tim->timer.function = (void (*)(unsigned long))callback;
	tim->timer.data = (unsigned long)data;
}

OSK_STATIC_INLINE void osk_timer_term(osk_timer *tim)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(0 ==	object_is_on_stack(tim));
	OSK_DEBUG_CODE(
		if (MALI_FALSE == tim->active)
		{
		}
	);
	/* Nothing to do */
}

OSK_STATIC_INLINE void osk_timer_on_stack_term(osk_timer *tim)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(0 !=	object_is_on_stack(tim));
	OSK_DEBUG_CODE(
		if (MALI_FALSE == tim->active)
		{
		}
	);
	destroy_timer_on_stack(&tim->timer);
}

#endif /* _OSK_ARCH_TIMERS_COMMERCIAL_H_ */
