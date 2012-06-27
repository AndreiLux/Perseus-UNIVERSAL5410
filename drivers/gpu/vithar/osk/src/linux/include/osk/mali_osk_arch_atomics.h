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

#ifndef _OSK_ARCH_ATOMICS_H_
#define _OSK_ARCH_ATOMICS_H_

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

OSK_STATIC_INLINE u32 osk_atomic_sub(osk_atomic * atom, u32 value)
{
	OSK_ASSERT(NULL != atom);
	return atomic_sub_return(value, atom);
}

OSK_STATIC_INLINE u32 osk_atomic_add(osk_atomic * atom, u32 value)
{
	OSK_ASSERT(NULL != atom);
	return atomic_add_return(value, atom);
}

OSK_STATIC_INLINE u32 osk_atomic_dec(osk_atomic * atom)
{
	OSK_ASSERT(NULL != atom);
	return osk_atomic_sub(atom, 1);
}

OSK_STATIC_INLINE u32 osk_atomic_inc(osk_atomic * atom)
{
	OSK_ASSERT(NULL != atom);
	return osk_atomic_add(atom, 1);
}

OSK_STATIC_INLINE void osk_atomic_set(osk_atomic * atom, u32 value)
{
	OSK_ASSERT(NULL != atom);
	atomic_set(atom, value);
}

OSK_STATIC_INLINE u32 osk_atomic_get(osk_atomic * atom)
{
	OSK_ASSERT(NULL != atom);
	return atomic_read(atom);
}

OSK_STATIC_INLINE u32 osk_atomic_compare_and_swap(osk_atomic * atom, u32 old_value, u32 new_value)
{
	OSK_ASSERT(NULL != atom);
	return atomic_cmpxchg(atom, old_value, new_value);
}

#endif /* _OSK_ARCH_ATOMICS_H_ */
