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
 * Base cross-proccess sync API.
 */

#ifndef _BASE_KERNEL_SYNC_H_
#define _BASE_KERNEL_SYNC_H_

#include <linux/ioctl.h>

#define STREAM_IOC_MAGIC '~'

/* Fence insert.
 *
 * Inserts a fence on the stream operated on.
 * Fence can be waited via a base fence wait soft-job
 * or triggered via a base fence trigger soft-job.
 *
 * Fences must be cleaned up with close when no longer needed.
 *
 * No input/output arguments.
 * Returns
 * >=0 fd
 * <0  error code
 */
#define STREAM_IOC_FENCE_INSERT _IO(STREAM_IOC_MAGIC, 0)

#endif /* _BASE_KERNEL_SYNC_H_ */

