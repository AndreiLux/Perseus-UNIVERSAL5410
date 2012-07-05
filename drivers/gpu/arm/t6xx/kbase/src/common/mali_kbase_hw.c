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

#include <kbase/mali_base_hwconfig.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include "mali_kbase.h"
#include "mali_kbase_hw.h"

mali_error kbase_hw_set_issues_mask(kbase_device *kbdev)
{
	const base_hw_issue *issues;

#if MALI_BACKEND_KERNEL || MALI_NO_MALI
	u32 gpu_id = kbase_os_reg_read(kbdev, GPU_CONTROL_REG(GPU_ID));

	switch (gpu_id)
	{
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 0, 0, GPU_ID_S_15DEV0):
		case GPU_ID_MAKE(GPU_ID_PI_T65X, 0, 0, GPU_ID_S_15DEV0):
			issues = base_hw_issues_t60x_t65x_r0p0_15dev0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 0, 0, GPU_ID_S_EAC):
		case GPU_ID_MAKE(GPU_ID_PI_T65X, 0, 0, GPU_ID_S_EAC):
			issues = base_hw_issues_t60x_t65x_r0p0_eac;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 0, 1, 0):
		case GPU_ID_MAKE(GPU_ID_PI_T65X, 0, 1, 0):
			issues = base_hw_issues_t60x_t65x_r0p1;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 1, 0, 0):
		case GPU_ID_MAKE(GPU_ID_PI_T65X, 1, 0, 0):
			issues = base_hw_issues_t60x_t65x_r1p0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T62X, 0, 0, 0):
			issues = base_hw_issues_t62x_r0p0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T67X, 0, 0, 0):
			issues = base_hw_issues_t67x_r0p0;
			break;
		default:
			OSK_PRINT_WARN(OSK_BASE_CORE, "Unknown GPU ID %x", gpu_id);
			return MALI_ERROR_FUNCTION_FAILED;
	}

	OSK_PRINT_INFO(OSK_BASE_CORE, "GPU identified as 0x%04x r%dp%d status %d",
				(gpu_id & GPU_ID_VERSION_PRODUCT_ID) >> GPU_ID_VERSION_PRODUCT_ID_SHIFT,
				(gpu_id & GPU_ID_VERSION_MAJOR) >> GPU_ID_VERSION_MAJOR_SHIFT,
				(gpu_id & GPU_ID_VERSION_MINOR) >> GPU_ID_VERSION_MINOR_SHIFT,
				(gpu_id & GPU_ID_VERSION_STATUS) >> GPU_ID_VERSION_STATUS_SHIFT);
#else
	/* We can only know that the model is used at compile-time */
	issues = base_hw_issues_model;
#endif

	for (; *issues != BASE_HW_ISSUE_END; issues++)
	{
		osk_bitarray_set_bit(*issues, &kbdev->hw_issues_mask[0]);
	}

	return MALI_ERROR_NONE;
}
