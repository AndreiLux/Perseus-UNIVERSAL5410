/*
 *
 * (C) COPYRIGHT 2011-2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#ifndef _BASE_HWCONFIG_H_
#define _BASE_HWCONFIG_H_

/* ----------------------------------------------------------------------------
   OBSOLETE ISSUES - WORKAROUNDS STILL EXIST IN CODEBASE, BUT NOT NEEDED
---------------------------------------------------------------------------- */
/* Clarifications requested to First Vertex Index. */
#define BASE_HW_ISSUE_999   0			/* Code in: GLES */

/* Incorrect handling of unorm16 pixel formats. */
#define BASE_HW_ISSUE_4015  0			/* Code in: GLES */

/* Framebuffer output smaller than 6 pixels causes hang. */
#define BASE_HW_ISSUE_5753  0			/* Code in: CFRAME, EGL, GLES */

/* Transaction Elimination doesn't work correctly. */
#define BASE_HW_ISSUE_5907  0			/* Code in: EGL */

/* Multisample write mask must be set to all 1s. */
#define BASE_HW_ISSUE_5936  0			/* Code in: GLES */

/* Jobs can get stuck after page fault */
#define BASE_HW_ISSUE_6035  0			/* Code in: CFRAME */

/* Hierarchical tiling doesn't work properly. */
#define BASE_HW_ISSUE_6097  0			/* Code in: CFRAME */

/* Depth texture read of D24S8 hangs the FPGA */
#define BASE_HW_ISSUE_6156  0			/* Code in: CFRAME, GLES */

/* Readback with negative stride doesn't work properly. */
#define BASE_HW_ISSUE_6325  0			/* Code in: CFRAME */

/* Using 8xMSAA surfaces produces incorrect output */
#define BASE_HW_ISSUE_6352  0			/* Code in: EGL */

/* Pixel format 95 doesn't work properly (HW writes to memory) */
#define BASE_HW_ISSUE_6405  0			/* Code in: CSTATE, CFRAME */

/* YUV image dimensions are specified in chroma samples, not luma samples. */
#define BASE_HW_ISSUE_6996  0			/* Code in: COBJ */

/* Writing to averaging mode MULTISAMPLE might hang */
#define BASE_HW_ISSUE_7516  0			/* Code in: CFRAME */

/* The whole tiler pointer array must be cleared */
#define BASE_HW_ISSUE_9102  0			/* Code in: CFRAME */

/* BASE_MEM_COHERENT_LOCAL does not work on beta HW */
#define BASE_HW_ISSUE_9235  0			/* Code in: CFRAME */

/* ----------------------------------------------------------------------------
   ACTIVE ISSUES
---------------------------------------------------------------------------- */
/* Tiler triggers a fault if the scissor rectangle is empty. */
#define BASE_HW_ISSUE_5699  1

/** The current version of the model doesn't support Soft-Stop */
#define BASE_HW_ISSUE_5736  0

/* Need way to guarantee that all previously-translated memory accesses are commited */
#define BASE_HW_ISSUE_6367  1

/* Unaligned load stores crossing 128 bit boundaries will fail */
#define BASE_HW_ISSUE_6402  1

/* On job complete with non-done the cache is not flushed */
#define BASE_HW_ISSUE_6787  1

/* The clamp integer coordinate flag bit of the sampler descriptor is reserved */
#define BASE_HW_ISSUE_7144  1

/* Write of PRFCNT_CONFIG_MODE_MANUAL to PRFCNT_CONFIG causes a instr. dump if PRFCNT_TILER_EN is enabled */
#define BASE_HW_ISSUE_8186  1

/* TIB: Reports faults from a vtile which has not yet been allocated */
#define BASE_HW_ISSUE_8245  1

/* Hierz doesn't work when stenciling is enabled */
#define BASE_HW_ISSUE_8260  1

/* Livelock in L0 icache */
#define BASE_HW_ISSUE_8280  1

/* uTLB deadlock could occur when writing to an invalid page at the same time as access to a valid page in the same uTLB cache line */
#define BASE_HW_ISSUE_8316  1

/* HT: TERMINATE for RUN command ignored if previous LOAD_DESCRIPTOR is still executing */
#define BASE_HW_ISSUE_8394  1

/* CSE : Sends a TERMINATED response for a task that should not be terminated */
/* (Note that PRLAM-8379 also uses this workaround) */
#define BASE_HW_ISSUE_8401  1

/* Repeatedly Soft-stopping a job chain consisting of (Vertex Shader, Cache Flush, Tiler) jobs causes 0x58 error on tiler job. */
#define BASE_HW_ISSUE_8408  1

/* Compute job hangs: disable the Pause buffer in the LS pipe.  */
/* BASE_HW_ISSUE_8443 implemented at runtime based on GPU ID (affects r0p0-00dev15) */

/* Stencil test enable 1->0 sticks */
#define BASE_HW_ISSUE_8456  1

/* Tiler heap issue using FBOs or multiple processes using the tiler simultaneously */
#define BASE_HW_ISSUE_8564  1

/* Livelock issue using atomic instructions (particularly when using atomic_cmpxchg as a spinlock) */
#define BASE_HW_ISSUE_8791  1

/* Jobs with relaxed dependencies are not supporting soft-stop */
#define BASE_HW_ISSUE_8803  1

/* Blend shader output is wrong for certain formats */
#define BASE_HW_ISSUE_8833  1

/* Occlusion queries can create false 0 result in boolean and counter modes */
#define BASE_HW_ISSUE_8879  1

/* Output has half intensity with blend shaders enabled on 8xMSAA. */
#define BASE_HW_ISSUE_8896  1

/* 8xMSAA does not work with CRC */
#define BASE_HW_ISSUE_8975  1

/* Boolean occlusion queries don't work properly due to sdc issue. */
#define BASE_HW_ISSUE_8986  0		/* TODO: Should be 1 according to JIRA? */

/* Change in RMUs in use causes problems related with the core's SDC */
#define BASE_HW_ISSUE_8987  1

/* Occlusion query result is not updated if color writes are disabled. */
#define BASE_HW_ISSUE_9010  0

/* Problem with number of work registers in the RSD if set to 0 */
#define BASE_HW_ISSUE_9275  1

/* Compute endpoint has a 4-deep queue of tasks, meaning a soft stop won't complete until all 4 tasks have completed */
#define BASE_HW_ISSUE_9435 1

#endif /* _BASE_HWCONFIG_H_ */
