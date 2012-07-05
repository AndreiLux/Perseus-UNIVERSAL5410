/*
 * This confidential and proprietary soft:q!ware may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_ukk_os.h
 * Types and definitions that are common for Linux OSs for the kernel side of the
 * User-Kernel interface.
 */

#ifndef _UKK_OS_H_ /* Linux version */
#define _UKK_OS_H_

#include <linux/fs.h>

/**
 * @addtogroup uk_api User-Kernel Interface API
 * @{
 */

/**
 * @addtogroup uk_api_kernel UKK (Kernel side)
 * @{
 */

/**
 * Internal OS specific data structure associated with each UKK session. Part
 * of a ukk_session object.
 */
typedef struct ukkp_session
{
	int dummy;     /**< No internal OS specific data at this time */
} ukkp_session;

/** @} end group uk_api_kernel */

/** @} end group uk_api */

#endif /* _UKK_OS_H__ */
