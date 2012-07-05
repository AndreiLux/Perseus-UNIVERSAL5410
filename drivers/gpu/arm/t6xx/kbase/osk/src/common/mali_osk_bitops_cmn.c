/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include <osk/mali_osk.h>

unsigned long osk_bitarray_find_first_zero_bit(const unsigned long *addr, unsigned long maxbit)
{
	unsigned long total;

	OSK_ASSERT(NULL != addr);

	for ( total = 0; total < maxbit; total += OSK_BITS_PER_LONG, ++addr )
	{
		if (OSK_ULONG_MAX != *addr)
		{
			int result;
			result = oskp_find_first_zero_bit( *addr );
			/* non-negative signifies the bit was found */
			if ( result >= 0 )
			{
				total += (unsigned long)result;
				break;
			}
		}
	}

	/* Now check if we reached maxbit or above */
	if ( total >= maxbit )
	{
		total = maxbit;
	}

	return total; /* either the found bit nr, or maxbit if not found */
}
