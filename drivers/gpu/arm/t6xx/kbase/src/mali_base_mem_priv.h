/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_MEM_PRIV_H_
#define _BASE_MEM_PRIV_H_

#define BASE_SYNCSET_OP_MSYNC	(1U << 0)
#define BASE_SYNCSET_OP_CSYNC	(1U << 1)

/*
 * This structure describe a basic memory coherency operation.
 * It can either be:
 * @li a sync from CPU to Memory:
 *	- type = ::BASE_SYNCSET_OP_MSYNC
 *	- mem_handle = a handle to the memory object on which the operation
 *	  is taking place
 *	- user_addr = the address of the range to be synced
 *	- size = the amount of data to be synced, in bytes
 *	- offset is ignored.
 * @li a sync from Memory to CPU:
 *	- type = ::BASE_SYNCSET_OP_CSYNC
 *	- mem_handle = a handle to the memory object on which the operation
 *	  is taking place
 *	- user_addr = the address of the range to be synced
 *	- size = the amount of data to be synced, in bytes.
 *	- offset is ignored.
 */
typedef struct basep_syncset
{
	mali_addr64 mem_handle;
	u64         user_addr;
	u32         size;
	u8          type;
} basep_syncset;

#endif
