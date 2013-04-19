/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>

#include "board-universal5410.h"

#define CORESIGHT_PHYS_BASE		0x10880000
#define CORESIGHT_ETB_PHYS_BASE		(CORESIGHT_PHYS_BASE + 0x1000)
#define CORESIGHT_TPIU_PHYS_BASE	(CORESIGHT_PHYS_BASE + 0x3000)
#define CORESIGHT_FUNNEL_PHYS_BASE	(CORESIGHT_PHYS_BASE + 0x4000)
#define CORESIGHT_ETM_PHYS_BASE		(CORESIGHT_PHYS_BASE + 0x1C000)

static struct resource coresight_etb_resources[] = {
	{
		.start = CORESIGHT_ETB_PHYS_BASE,
		.end   = CORESIGHT_ETB_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device coresight_etb_device = {
	.name          = "coresight_etb",
	.id            = -1,
	.num_resources = ARRAY_SIZE(coresight_etb_resources),
	.resource      = coresight_etb_resources,
};

static struct resource coresight_tpiu_resources[] = {
	{
		.start = CORESIGHT_TPIU_PHYS_BASE,
		.end   = CORESIGHT_TPIU_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device coresight_tpiu_device = {
	.name          = "coresight_tpiu",
	.id            = -1,
	.num_resources = ARRAY_SIZE(coresight_tpiu_resources),
	.resource      = coresight_tpiu_resources,
};

static struct resource coresight_funnel_resources[] = {
	{
		.start = CORESIGHT_FUNNEL_PHYS_BASE,
		.end   = CORESIGHT_FUNNEL_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device coresight_funnel_device = {
	.name          = "coresight_funnel",
	.id            = -1,
	.num_resources = ARRAY_SIZE(coresight_funnel_resources),
	.resource      = coresight_funnel_resources,
};

static struct resource coresight_etm_resources[] = {
	{
		.start = CORESIGHT_ETM_PHYS_BASE,
		.end   = CORESIGHT_ETM_PHYS_BASE + (SZ_4K * 2) - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device coresight_etm_device = {
	.name          = "coresight_etm",
	.id            = -1,
	.num_resources = ARRAY_SIZE(coresight_etm_resources),
	.resource      = coresight_etm_resources,
};

static struct platform_device *coresight_devices[] __initdata = {
	&coresight_etb_device,
	&coresight_tpiu_device,
	&coresight_funnel_device,
	&coresight_etm_device,
};

void __init exynos5_universal5410_coresight_init(void) {
	printk(KERN_ERR "[%s] initialization start!\n", __func__);
	platform_add_devices(coresight_devices,
			ARRAY_SIZE(coresight_devices));
};
