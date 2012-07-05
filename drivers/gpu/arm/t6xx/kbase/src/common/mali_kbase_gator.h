/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI_GATOR_SUPPORT
#define MALI_GATOR_SUPPORT 0
#endif

#if MALI_GATOR_SUPPORT
#define GATOR_MAKE_EVENT(type,number) (((type) << 24) | ((number) << 16))
#define GATOR_JOB_SLOT_START 1
#define GATOR_JOB_SLOT_STOP  2
#define GATOR_JOB_SLOT_SOFT_STOPPED  3
void kbase_trace_mali_job_slots_event(u32 event, const kbase_context * kctx);
void kbase_trace_mali_pm_status(u32 event, u64 value);
void kbase_trace_mali_pm_power_off(u32 event, u64 value);
void kbase_trace_mali_pm_power_on(u32 event, u64 value);
void kbase_trace_mali_page_fault_insert_pages(int event, u32 value);
void kbase_trace_mali_mmu_as_in_use(int event);
void kbase_trace_mali_mmu_as_released(int event);
void kbase_trace_mali_total_alloc_pages_change(long long int event);
#endif
