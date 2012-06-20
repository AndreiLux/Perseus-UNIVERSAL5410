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

#ifndef _OSK_ARCH_BITOPS_H_
#define _OSK_ARCH_BITOPS_H_

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

#include <linux/bitops.h>

OSK_STATIC_INLINE long osk_clz(unsigned long val)
{
	return OSK_BITS_PER_LONG - fls_long(val);
}

OSK_STATIC_INLINE long osk_clz_64(u64 val)
{
	return 64 - fls64(val);
}

OSK_STATIC_INLINE int osk_count_set_bits(unsigned long val)
{
	/* note: __builtin_popcountl() not available in kernel */
	int count = 0;
	while (val)
	{
		count++;
		val &= (val-1);
	}
	return count;
}

#endif /* _OSK_ARCH_BITOPS_H_ */
