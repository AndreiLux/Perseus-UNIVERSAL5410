/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_ARCH_MEM_H_
#define _OSK_ARCH_MEM_H_

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

#include <linux/slab.h>
#include <linux/vmalloc.h>

OSK_STATIC_INLINE void * osk_malloc(size_t size)
{
	OSK_ASSERT(0 != size);

	if(OSK_SIMULATE_FAILURE(OSK_OSK))
	{
		return NULL;
	}

	return kmalloc(size, GFP_KERNEL);
}

OSK_STATIC_INLINE void * osk_calloc(size_t size)
{
	OSK_ASSERT(0 != size);

	if(OSK_SIMULATE_FAILURE(OSK_OSK))
	{
		return NULL;
	}

	return kzalloc(size, GFP_KERNEL);
}

OSK_STATIC_INLINE void osk_free(void * ptr)
{
	kfree(ptr);
}

OSK_STATIC_INLINE void * osk_vmalloc(size_t size)
{
	OSK_ASSERT(0 != size);

	if(OSK_SIMULATE_FAILURE(OSK_OSK))
	{
		return NULL;
	}

	return vmalloc_user(size);
}

OSK_STATIC_INLINE void osk_vfree(void * ptr)
{
	vfree(ptr);
}

#define OSK_MEMCPY( dst, src, len ) memcpy(dst, src, len)

#define OSK_MEMCMP( s1, s2, len ) memcmp(s1, s2, len)

#define OSK_MEMSET( ptr, chr, size ) memset(ptr, chr, size)


#endif /* _OSK_ARCH_MEM_H_ */
