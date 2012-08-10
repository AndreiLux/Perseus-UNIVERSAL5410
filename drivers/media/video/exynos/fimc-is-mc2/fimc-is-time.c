#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <linux/cma.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>

#include "fimc-is-time.h"

#ifdef MEASURE_TIME
struct timeval old_time;

int g_shot_period(struct timeval *str)
{
	u32 shot_period;

	shot_period = (str->tv_sec - old_time.tv_sec)*1000000 +
				(str->tv_usec - old_time.tv_usec);

	old_time = *str;

	return shot_period;
}

int g_shot_time(struct timeval *str, struct timeval *end)
{
	u32 shot_time;

	shot_time = (end->tv_sec - str->tv_sec)*1000000 +
				(end->tv_usec - str->tv_usec);

	return shot_time;
}

int g_meta_time(struct timeval *str, struct timeval *end)
{
	u32 meta_time;

	meta_time = (end->tv_sec - str->tv_sec)*1000000 +
				(end->tv_usec - str->tv_usec);

	return meta_time;
}
#endif
