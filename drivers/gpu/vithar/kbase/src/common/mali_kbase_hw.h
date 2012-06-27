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
 * @file
 * Run-time work-arounds helpers
 */

#ifndef _KBASE_HW_H_
#define _KBASE_HW_H_

#include <osk/mali_osk.h>
#include "mali_kbase_defs.h"

/**
 * @brief Tell whether a work-around should be enabled
 */
#define kbase_hw_has_issue(kbdev, issue)\
        osk_bitarray_test_bit(issue, &(kbdev)->hw_issues_mask[0])

/**
 * @brief Set the HW issues mask depending on the GPU ID
 */
mali_error kbase_hw_set_issues_mask(kbase_device *kbdev);

#endif /* _KBASE_HW_H_ */
