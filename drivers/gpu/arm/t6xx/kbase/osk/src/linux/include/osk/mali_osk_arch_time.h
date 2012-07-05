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

#ifndef _OSK_ARCH_TIME_H
#define _OSK_ARCH_TIME_H

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

OSK_STATIC_INLINE osk_ticks osk_time_now(void)
{
	return jiffies;
}

OSK_STATIC_INLINE u32 osk_time_mstoticks(u32 ms)
{
	return msecs_to_jiffies(ms);
}

OSK_STATIC_INLINE u32 osk_time_elapsed(osk_ticks ticka, osk_ticks tickb)
{
	return jiffies_to_msecs((long)tickb - (long)ticka);
}

OSK_STATIC_INLINE mali_bool osk_time_after(osk_ticks ticka, osk_ticks tickb)
{
	return time_after(ticka, tickb);
}

OSK_STATIC_INLINE void osk_gettimeofday(osk_timeval *tv)
{
	struct timespec ts;
	getnstimeofday(&ts);

	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec/1000;
}

#endif /* _OSK_ARCH_TIME_H_ */
