/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Base Scaler resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/map.h>

static struct resource exynos5_scaler_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_SCALER, SZ_4K),
	[1] = DEFINE_RES_IRQ(EXYNOS5_IRQ_SCALER),
};

struct platform_device exynos5_device_scaler = {
	.name		= "exynos5-scaler",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos5_scaler_resource),
	.resource	= exynos5_scaler_resource,
};
