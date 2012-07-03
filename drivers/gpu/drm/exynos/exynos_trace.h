#if !defined(_EXYNOS_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _EXYNOS_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#include <drm/drmP.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos
#define TRACE_SYSTEM_STRING __stringify(TRACE_SYSTEM)
#define TRACE_INCLUDE_FILE exynos_trace

/* object tracking */

TRACE_EVENT(exynos_flip_request,
	TP_PROTO(unsigned int pipe),
	TP_ARGS(pipe),

	TP_STRUCT__entry(
		__field(unsigned int, pipe)
	),

	TP_fast_assign(
		__entry->pipe = pipe;
	),

	TP_printk("pipe=%u", __entry->pipe)
);

TRACE_EVENT(exynos_flip_complete,
	TP_PROTO(unsigned int pipe),
	TP_ARGS(pipe),

	TP_STRUCT__entry(
		__field(unsigned int, pipe)
	),

	TP_fast_assign(
		__entry->pipe = pipe;
	),

	TP_printk("pipe=%u", __entry->pipe)
);

#endif /* _EXYNOS_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

