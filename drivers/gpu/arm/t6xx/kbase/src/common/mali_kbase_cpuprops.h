/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_cpuprops.h
 * Base kernel property query APIs
 */

#ifndef _KBASE_CPUPROPS_H_
#define _KBASE_CPUPROPS_H_

#include <malisw/mali_malisw.h>

/* Forward declarations */
struct kbase_context;
struct kbase_uk_cpuprops;

/**
 * @brief Default implementation of @ref KBASE_CONFIG_ATTR_CPU_SPEED_FUNC.
 *
 * This function sets clock_speed to 100, so will be an underestimate for
 * any real system.
 *
 * See @refkbase_cpuprops_clock_speed_function for details on the parameters
 * and return value.
 */
int kbase_cpuprops_get_default_clock_speed(u32 *clock_speed);

/**
 * @brief Provides CPU properties data.
 *
 * Fill the kbase_uk_cpuprops with values from CPU configuration.
 *
 * @param kctx         The kbase context
 * @param kbase_props  A copy of the kbase_uk_cpuprops structure from userspace
 *
 * @return MALI_ERROR_NONE on success. Any other value indicates failure.
 */
mali_error kbase_cpuprops_uk_get_props(struct kbase_context *kctx, struct kbase_uk_cpuprops* kbase_props);

#endif /*_KBASE_CPUPROPS_H_*/
