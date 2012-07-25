#undef TRACE_SYSTEM
#define TRACE_SYSTEM mali

#if !defined(_TRACE_MALI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MALI_H

#include <linux/tracepoint.h>

TRACE_EVENT(mali_dvfs_event,
	    TP_PROTO(unsigned int util),
	    TP_ARGS(util),
	    TP_STRUCT__entry(
		    __field(unsigned int, utilization)
	    ),
	    TP_fast_assign(
		    __entry->utilization = util;
	    ),
	    TP_printk("utilization=%u", __entry->utilization)
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

#endif /* _TRACE_MALI_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
