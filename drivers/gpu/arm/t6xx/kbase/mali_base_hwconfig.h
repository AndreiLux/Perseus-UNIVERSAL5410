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
 * Software workarounds configuration for Hardware issues.
 */

#ifndef _BASE_HWCONFIG_H_
#define _BASE_HWCONFIG_H_

#include <malisw/mali_malisw.h>

/**
 * List of all workarounds.
 *
 */

typedef enum base_hw_issue {

	/* Tiler triggers a fault if the scissor rectangle is empty. */
	BASE_HW_ISSUE_5699,

	/* The current version of the model doesn't support Soft-Stop */
	BASE_HW_ISSUE_5736,

	/* Need way to guarantee that all previously-translated memory accesses are commited */
	BASE_HW_ISSUE_6367,

	/* Unaligned load stores crossing 128 bit boundaries will fail */
	BASE_HW_ISSUE_6402,

	/* On job complete with non-done the cache is not flushed */
	BASE_HW_ISSUE_6787,

	/* The clamp integer coordinate flag bit of the sampler descriptor is reserved */
	BASE_HW_ISSUE_7144,

	/* Write of PRFCNT_CONFIG_MODE_MANUAL to PRFCNT_CONFIG causes a instrumentation dump if
	   PRFCNT_TILER_EN is enabled */
	BASE_HW_ISSUE_8186,

	/* TIB: Reports faults from a vtile which has not yet been allocated */
	BASE_HW_ISSUE_8245,

	/* WLMA memory goes wrong when run on shader cores other than core 0. */
	BASE_HW_ISSUE_8250,

	/* Hierz doesn't work when stenciling is enabled */
	BASE_HW_ISSUE_8260,

	/* Livelock in L0 icache */
	BASE_HW_ISSUE_8280,

	/* uTLB deadlock could occur when writing to an invalid page at the same time as
	 * access to a valid page in the same uTLB cache line ( == 4 PTEs == 16K block of mapping) */
	BASE_HW_ISSUE_8316,

	/* HT: TERMINATE for RUN command ignored if previous LOAD_DESCRIPTOR is still executing */
	BASE_HW_ISSUE_8394,

	/* CSE : Sends a TERMINATED response for a task that should not be terminated */
	/* (Note that PRLAM-8379 also uses this workaround) */
	BASE_HW_ISSUE_8401,

	/* Repeatedly Soft-stopping a job chain consisting of (Vertex Shader, Cache Flush, Tiler)
	 * jobs causes 0x58 error on tiler job. */
	BASE_HW_ISSUE_8408,

	/* Disable the Pause Buffer in the LS pipe. */
	BASE_HW_ISSUE_8443,

	/* Stencil test enable 1->0 sticks */
	BASE_HW_ISSUE_8456,

	/* Tiler heap issue using FBOs or multiple processes using the tiler simultaneously */
	BASE_HW_ISSUE_8564,

	/* Livelock issue using atomic instructions (particularly when using atomic_cmpxchg as a spinlock) */
	BASE_HW_ISSUE_8791,

	/* Fused jobs are not supported (for various reasons) */
	/* Jobs with relaxed dependencies do not support soft-stop */
	/* (Note that PRLAM-8803, PRLAM-8393, PRLAM-8559, PRLAM-8601 & PRLAM-8607 all use this work-around) */
	BASE_HW_ISSUE_8803,

	/* Blend shader output is wrong for certain formats */
	BASE_HW_ISSUE_8833,

	/* Occlusion queries can create false 0 result in boolean and counter modes */
	BASE_HW_ISSUE_8879,

	/* Output has half intensity with blend shaders enabled on 8xMSAA. */
	BASE_HW_ISSUE_8896,

	/* 8xMSAA does not work with CRC */
	BASE_HW_ISSUE_8975,

	/* Boolean occlusion queries don't work properly due to sdc issue. */
	BASE_HW_ISSUE_8986,

	/* Change in RMUs in use causes problems related with the core's SDC */
	BASE_HW_ISSUE_8987,

	/* Occlusion query result is not updated if color writes are disabled. */
	BASE_HW_ISSUE_9010,

	/* Problem with number of work registers in the RSD if set to 0 */
	BASE_HW_ISSUE_9275,

	/* Compute endpoint has a 4-deep queue of tasks, meaning a soft stop won't complete until all 4 tasks have completed */
	BASE_HW_ISSUE_9435,

	/* HT: Tiler returns TERMINATED for command that hasn't been terminated */
	BASE_HW_ISSUE_9510,

	/* Occasionally the GPU will issue multiple page faults for the same address before the MMU page table has been read by the GPU */
	BASE_HW_ISSUE_9630,

	/* The BASE_HW_ISSUE_END value must be the last issue listed in this enumeration
	 * and must be the last value in each array that contains the list of workarounds
	 * for a particular HW version.
	 */
	BASE_HW_ISSUE_END
} base_hw_issue;


/**
 * Workarounds configuration for each HW revision
 */
#if MALI_BACKEND_KERNEL || MALI_NO_MALI
/* Mali T60x/T65x r0p0-15dev0 - 2011-W39-stable-9 */
static const base_hw_issue base_hw_issues_t60x_t65x_r0p0_15dev0[] =
{
	BASE_HW_ISSUE_5699,
	BASE_HW_ISSUE_6367,
	BASE_HW_ISSUE_6402,
	BASE_HW_ISSUE_6787,
	BASE_HW_ISSUE_7144,
	BASE_HW_ISSUE_8186,
	BASE_HW_ISSUE_8245,
	BASE_HW_ISSUE_8250,
	BASE_HW_ISSUE_8260,
	BASE_HW_ISSUE_8280,
	BASE_HW_ISSUE_8316,
	BASE_HW_ISSUE_8394,
	BASE_HW_ISSUE_8401,
	BASE_HW_ISSUE_8408,
	BASE_HW_ISSUE_8443,
	BASE_HW_ISSUE_8456,
	BASE_HW_ISSUE_8564,
	BASE_HW_ISSUE_8791,
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_8833,
	BASE_HW_ISSUE_8896,
	BASE_HW_ISSUE_8975,
	BASE_HW_ISSUE_8986,
	BASE_HW_ISSUE_8987,
	BASE_HW_ISSUE_9010,
	BASE_HW_ISSUE_9275,
	BASE_HW_ISSUE_9435,
	BASE_HW_ISSUE_9510,
	BASE_HW_ISSUE_9630,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

/* Mali T60x/T65x r0p0-00rel0 - 2011-W46-stable-13c */
static const base_hw_issue base_hw_issues_t60x_t65x_r0p0_eac[] =
{
	BASE_HW_ISSUE_5699,
	BASE_HW_ISSUE_6367,
	BASE_HW_ISSUE_6402,
	BASE_HW_ISSUE_6787,
	BASE_HW_ISSUE_8564,
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_8975,
	BASE_HW_ISSUE_9010,
	BASE_HW_ISSUE_9275,
	BASE_HW_ISSUE_9435,
	BASE_HW_ISSUE_9510,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

/* Mali T60x/T65x r0p1 */
static const base_hw_issue base_hw_issues_t60x_t65x_r0p1[] =
{
	BASE_HW_ISSUE_5699,
	BASE_HW_ISSUE_6367,
	BASE_HW_ISSUE_6402,
	BASE_HW_ISSUE_6787,
	BASE_HW_ISSUE_8564,
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_8975,
	BASE_HW_ISSUE_9010,
	BASE_HW_ISSUE_9275,
	BASE_HW_ISSUE_9435,
	BASE_HW_ISSUE_9510,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

/* Mali T60x/T65x r1p0-00rel0 */
static const base_hw_issue base_hw_issues_t60x_t65x_r1p0[] =
{
	BASE_HW_ISSUE_6367,
	BASE_HW_ISSUE_6402,
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_9435,
	BASE_HW_ISSUE_9510,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

/* Mali T62x r0p0 */
static const base_hw_issue base_hw_issues_t62x_r0p0[] =
{
	BASE_HW_ISSUE_6367,
	BASE_HW_ISSUE_6402,
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_9435,
	BASE_HW_ISSUE_9510,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

/* Mali T67x r0p0 */
static const base_hw_issue base_hw_issues_t67x_r0p0[] =
{
	BASE_HW_ISSUE_6367,
	BASE_HW_ISSUE_6402,
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_9435,
	BASE_HW_ISSUE_9510,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

#else /* MALI_BACKEND_KERNEL || MALI_NO_MALI */

/* Model configuration
 *
 * note: We can only know that the model is used at compile-time
 */

static const base_hw_issue base_hw_issues_model[] =
{
	BASE_HW_ISSUE_5736,
	BASE_HW_ISSUE_6402, /* NOTE: Fix is present in model r125162 but is not enabled until RTL is fixed */
	BASE_HW_ISSUE_8803,
	BASE_HW_ISSUE_8975, /* NOTE: Model is fixed for BASE_HW_ISSUE_8975, but not EGL, see MIDEGL-868 */
	BASE_HW_ISSUE_9010,
	BASE_HW_ISSUE_9275,
	BASE_HW_ISSUE_9435,
	/* List of hardware issues must end with BASE_HW_ISSUE_END */
	BASE_HW_ISSUE_END
};

#endif  /* MALI_BACKEND_KERNEL || MALI_NO_MALI */

#endif /* _BASE_HWCONFIG_H_ */
