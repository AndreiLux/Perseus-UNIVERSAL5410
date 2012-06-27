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
 * @file mali_osk_math.h
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_MATH_H_
#define _OSK_MATH_H_

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @addtogroup base_api
 * @{
 */

/**
 * @addtogroup base_osk_api
 * @{
 */

/**
 * @addtogroup oskmath Math
 *
 * Math related functions for which no commmon behavior exists on OS.
 *
 * @{
 */

/**
 * @brief Divide a 64-bit value with a 32-bit divider
 *
 * Performs an (unsigned) integer division of a 64-bit value
 * with a 32-bit divider and returns the 64-bit result and
 * 32-bit remainder.
 *
 * Provided as part of the OSK as not all OSs support 64-bit
 * division in an uniform way. Currently required to support
 * printing 64-bit numbers in the OSK debug message functions.
 *
 * @param[in,out] value   pointer to a 64-bit value to be divided by
 *                        \a divisor. The integer result of the division
 *                        is stored in \a value on output.
 * @param[in]     divisor 32-bit divisor
 * @return 32-bit remainder of the division
 */
OSK_STATIC_INLINE u32 osk_divmod6432(u64 *value, u32 divisor);

/** @} */  /* end group oskmath */

/** @} */ /* end group base_osk_api */

/** @} */ /* end group base_api */

/* pull in the arch header with the implementation  */
#include <osk/mali_osk_arch_math.h>

#ifdef __cplusplus
}
#endif

#endif /* _OSK_MATH_H_ */
