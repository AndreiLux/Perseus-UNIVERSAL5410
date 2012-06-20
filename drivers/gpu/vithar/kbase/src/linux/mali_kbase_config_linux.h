/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _KBASE_CONFIG_LINUX_H_
#define _KBASE_CONFIG_LINUX_H_

#include <kbase/mali_kbase_config.h>
#include <linux/ioport.h>

#define PLATFORM_CONFIG_RESOURCE_COUNT 4
#define PLATFORM_CONFIG_IRQ_RES_COUNT  3

#if !MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE
/**
 * @brief Convert data in kbase_io_resources struct to Linux-specific resources
 *
 * Function converts data in kbase_io_resources struct to an array of Linux resource structures. Note that function
 * assumes that size of linux_resource array is at least PLATFORM_CONFIG_RESOURCE_COUNT.
 * Resources are put in fixed order: I/O memory region, job IRQ, MMU IRQ, GPU IRQ.
 *
 * @param[in]  io_resource      Input IO resource data
 * @param[out] linux_resources  Pointer to output array of Linux resource structures
 */
void kbasep_config_parse_io_resources(const kbase_io_resources *io_resource, struct resource *linux_resources);
#endif /* !MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE */


#endif /* _KBASE_CONFIG_LINUX_H_ */
