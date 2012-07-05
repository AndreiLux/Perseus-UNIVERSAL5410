/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *       (C) COPYRIGHT 2009-2010, 2012 ARM Limited , ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized copies and
 * copies may only be made to the extent permitted by a licensing agreement
 * from ARM Limited.
 * ----------------------------------------------------------------------------
 */

#ifndef _ARM_CSTD_TYPES_H_
#define _ARM_CSTD_TYPES_H_

#if 1 == CSTD_TOOLCHAIN_MSVC
	#include "arm_cstd_types_msvc.h"
#elif 1 == CSTD_TOOLCHAIN_GCC
	#include "arm_cstd_types_gcc.h"
#elif 1 == CSTD_TOOLCHAIN_RVCT
	#include "arm_cstd_types_rvct.h"
#else
	#error "Toolchain not recognized"
#endif

#endif /* End (_ARM_CSTD_TYPES_H_) */
