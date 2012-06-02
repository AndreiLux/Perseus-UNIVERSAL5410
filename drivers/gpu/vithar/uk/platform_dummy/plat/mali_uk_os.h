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
 * @file mali_uk_os.h
 * User-Kernel Interface (kernel and user-side) dependent APIs (Linux).
 */

#ifndef _UK_OS_H_ /* Linux */
#define _UK_OS_H_

#ifdef MALI_DEBUG
#define MALI_UK_CANARY_VALUE    0xb2bdbdf6
#endif

#if MALI_BACKEND_KERNEL

#define LINUX_UK_BASE_MAGIC 0x80 /* BASE UK ioctl */

struct uku_context
{
	struct
	{
#ifdef MALI_DEBUG
		u32 canary;
#endif
		int fd;
	} ukup_internal_struct;
};

#else /* MALI_BACKEND_KERNEL */

typedef struct ukk_userspace
{
	void * ctx;
	mali_error (*dispatch)(void * /*ctx*/, void* /*msg*/, u32 /*size*/);
	void (*close)(struct ukk_userspace * /*self*/);
} ukk_userspace;

typedef ukk_userspace * (*kctx_open)(void);

struct uku_context
{
	struct
	{
#ifdef MALI_DEBUG
		u32 canary;
#endif
		ukk_userspace * ukku;
	} ukup_internal_struct;
};

#endif /* MALI_BACKEND_KERNEL */

#endif /* _UK_OS_H_ */
