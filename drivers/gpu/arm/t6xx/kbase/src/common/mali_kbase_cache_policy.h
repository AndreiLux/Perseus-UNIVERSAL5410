/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_cache_policy.h
 * Cache Policy API.
 */

#ifndef _KBASE_CACHE_POLICY_H_
#define _KBASE_CACHE_POLICY_H_

#include <malisw/mali_malisw.h>
#include "mali_kbase.h"
#include <kbase/mali_base_kernel.h>

/**
 * @brief Choose the cache policy for a specific region
 *
 * Tells whether the CPU and GPU caches should be enabled or not for a specific region.
 * This function can be modified to customize the cache policy depending on the flags
 * and size of the region.
 *
 * @param[in] flags     flags describing attributes of the region
 * @param[in] nr_pages  total number of pages (backed or not) for the region
 *
 * @return a combination of KBASE_REG_CPU_CACHED and KBASE_REG_GPU_CACHED depending
 * on the cache policy
 */
u32 kbase_cache_enabled(u32 flags, u32 nr_pages);

#endif /* _KBASE_CACHE_POLICY_H_ */
