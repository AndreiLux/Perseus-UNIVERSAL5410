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

#if !defined(_TRACE_MALI_DVFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MALI_DVFS_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mali_dvfs

#include <linux/tracepoint.h>

TRACE_EVENT(mali_dvfs_event,
	    TP_PROTO(unsigned int util, int avg),
	    TP_ARGS(util, avg),
	    TP_STRUCT__entry(
		    __field(unsigned int, utilization)
		    __field(int, avg_utilization)
	    ),
	    TP_fast_assign(
		    __entry->utilization = util;
		    __entry->avg_utilization = avg;
	    ),
	    TP_printk("utilization=%u avg=%d",
			__entry->utilization, __entry->avg_utilization)
);

TRACE_EVENT(mali_dvfs_set_voltage,
	    TP_PROTO(unsigned int vol),
	    TP_ARGS(vol),
	    TP_STRUCT__entry(
		    __field(unsigned int, voltage)
	    ),
	    TP_fast_assign(
		    __entry->voltage = vol;
	    ),
	    TP_printk("voltage=%u", __entry->voltage)
);

TRACE_EVENT(mali_dvfs_set_clock,
	    TP_PROTO(int freq),
	    TP_ARGS(freq),
	    TP_STRUCT__entry(
		    __field(int, frequency)
	    ),
	    TP_fast_assign(
		    __entry->frequency = freq;
	    ),
	    TP_printk("frequency=%d", __entry->frequency)
);

#endif /* _TRACE_MALI_DVFS_H */

#undef TRACE_INCLUDE_PATH
#undef linux
#define TRACE_INCLUDE_PATH MALI_KBASE_SRC_LINUX_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mali_linux_dvfs_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
