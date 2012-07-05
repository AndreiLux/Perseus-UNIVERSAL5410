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
 * @file
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_ARCH_POWER_H_
#define _OSK_ARCH_POWER_H_

#include <linux/pm_runtime.h>

OSK_STATIC_INLINE osk_power_request_result osk_power_request(osk_power_info *info, osk_power_state state)
{
	osk_power_request_result request_result = OSK_POWER_REQUEST_FINISHED;

	OSK_ASSERT(NULL != info);

	switch(state)
	{
		case OSK_POWER_STATE_OFF:
			/* request OS to suspend device*/
			break;
		case OSK_POWER_STATE_IDLE:
			/* request OS to idle device */
			break;
		case OSK_POWER_STATE_ACTIVE:
			/* request OS to resume device */
			break;
	}
	return request_result;
}

#endif
