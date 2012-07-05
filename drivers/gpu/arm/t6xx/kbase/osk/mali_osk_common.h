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
 * @file mali_osk_common.h
 * This header defines macros shared by the Common Layer public interfaces for
 * all utilities, to ensure they are available even if a client does not include
 * the convenience header mali_osk.h.
 */

#ifndef _OSK_COMMON_H_
#define _OSK_COMMON_H_

#include <osk/include/mali_osk_debug.h>

/**
 * @private
 */
static INLINE mali_bool oskp_ptr_is_null(const void* ptr)
{
	CSTD_UNUSED(ptr);
	OSK_ASSERT(ptr != NULL);
	return MALI_FALSE;
}

/**
 * @brief Check if a pointer is NULL, if so an assert is triggered, otherwise the pointer itself is returned.
 *
 * @param [in] ptr Pointer to test
 *
 * @return @c ptr if it's not NULL.
 */
#define OSK_CHECK_PTR(ptr)\
	(oskp_ptr_is_null(ptr) ? NULL : ptr)

#endif /* _OSK_COMMON_H_ */
