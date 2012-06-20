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

#ifndef _OSK_ARCH_MATH_H
#define _OSK_ARCH_MATH_H

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

#include <asm/div64.h>

OSK_STATIC_INLINE u32 osk_divmod6432(u64 *value, u32 divisor)
{
	u64 v;
	u32 r;

	OSK_ASSERT(NULL != value);

	v = *value;
	r = do_div(v, divisor);
	*value = v;
	return r;
}

#endif /* _OSK_ARCH_MATH_H_ */
