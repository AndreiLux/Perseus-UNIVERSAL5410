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
 * @file
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_ARCH_TIMERS_H
#define _OSK_ARCH_TIMERS_H

#if MALI_LICENSE_IS_GPL
	#include "mali_osk_arch_timers_gpl.h"
#else
	#include "mali_osk_arch_timers_commercial.h"
#endif

#endif /* _OSK_ARCH_TIMERS_H_ */
