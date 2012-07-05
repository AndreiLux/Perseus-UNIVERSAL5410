/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_gpuprops.h
 * Base kernel property query APIs
 */

#ifndef _KBASE_GPUPROPS_H_
#define _KBASE_GPUPROPS_H_

#include "mali_kbase_gpuprops_types.h"

/* Forward definition - see mali_kbase.h */
struct kbase_device;
struct kbase_context;

/**
 * @brief Set up Kbase GPU properties.
 *
 * Set up Kbase GPU properties with information from the GPU registers
 *
 * @param kbdev 	The kbase_device structure for the device
 */
void kbase_gpuprops_set(struct kbase_device *kbdev);

/**
 * @brief Provide GPU properties to userside through UKU call.
 *
 * Fill the kbase_uk_gpuprops with values from GPU configuration registers.
 *
 * @param kctx 		The kbase_context structure
 * @param kbase_props 	A copy of the kbase_uk_gpuprops structure from userspace
 *
 * @return MALI_ERROR_NONE on success. Any other value indicates failure.
 */
mali_error  kbase_gpuprops_uk_get_props(struct kbase_context *kctx, kbase_uk_gpuprops * kbase_props);


#endif /* _KBASE_GPUPROPS_H_ */
