/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _OSK_H_
#define _OSK_H_

/**
 * @addtogroup base_api
 * @{
 */

/**
 * @defgroup base_osk_api Kernel-side OSK APIs
 */

/** @} */ /* end group base_api */

#include "include/mali_osk_types.h"
#include "include/mali_osk_debug.h"
#if (1== MALI_BASE_TRACK_MEMLEAK)
#include "include/mali_osk_failure.h"
#endif
#include "include/mali_osk_math.h"
#include "include/mali_osk_lists.h"
#include "include/mali_osk_lock_order.h"
#include "include/mali_osk_locks.h"
#include "include/mali_osk_atomics.h"
#include "include/mali_osk_timers.h"
#include "include/mali_osk_time.h"
#include "include/mali_osk_bitops.h"
#include "include/mali_osk_workq.h"
#include "include/mali_osk_mem.h"
#include "include/mali_osk_low_level_mem.h"
#if (1 == MALI_BASE_TRACK_MEMLEAK)
#include "include/mali_osk_mem_wrappers.h"
#endif
#include "include/mali_osk_waitq.h"
#include "include/mali_osk_power.h"
#include "include/mali_osk_credentials.h"

#endif /* _OSK_H_ */
