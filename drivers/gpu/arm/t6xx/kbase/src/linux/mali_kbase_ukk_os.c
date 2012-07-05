/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/kernel.h>  /* Needed for KERN_INFO */
#include <linux/init.h>    /* Needed for the macros */

#include <osk/mali_osk.h>
#include <kbase/mali_ukk.h>

mali_error ukk_session_init(ukk_session *ukk_session, ukk_dispatch_function dispatch, u16 version_major, u16 version_minor)
{
	OSK_ASSERT(NULL != ukk_session);
	OSK_ASSERT(NULL != dispatch);

	/* OS independent initialization of UKK context */
	ukk_session->dispatch = dispatch;
	ukk_session->version_major = version_major;
	ukk_session->version_minor = version_minor;

	/* OS specific initialization of UKK context */
	ukk_session->internal_session.dummy = 0;
	return MALI_ERROR_NONE;
}

void ukk_session_term(ukk_session *ukk_session)
{
	OSK_ASSERT(NULL != ukk_session);
}

mali_error ukk_copy_from_user( size_t bytes,  void * kernel_buffer, const void * const user_buffer )
{
	if ( copy_from_user( kernel_buffer, user_buffer, bytes ) )
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}
	return MALI_ERROR_NONE;
}

mali_error ukk_copy_to_user( size_t bytes, void * user_buffer, const void * const kernel_buffer )
{
	if ( copy_to_user( user_buffer, kernel_buffer, bytes ) )
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}
	return MALI_ERROR_NONE;
}

