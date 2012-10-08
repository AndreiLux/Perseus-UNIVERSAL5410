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



#ifndef MALI_GATOR_SUPPORT
#define MALI_GATOR_SUPPORT 0
#endif

#if MALI_GATOR_SUPPORT
#define GATOR_MAKE_EVENT(type,number) (((type) << 24) | ((number) << 16))
#define GATOR_JOB_SLOTS_START 1
#define GATOR_JOB_SLOTS_STOP  2
void kbase_trace_mali_job_slots_event(u32 event);
#endif

