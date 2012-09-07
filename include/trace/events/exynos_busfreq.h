#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos_busfreq

#if !defined(_TRACE_EXYNOS_BUSFREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXYNOS_BUSFREQ_H

#include <linux/tracepoint.h>

TRACE_EVENT(exynos_busfreq_target_int,
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

TRACE_EVENT(exynos_busfreq_target_mif,
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

#endif /* _TRACE_EXYNOS_BUSFREQ_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
