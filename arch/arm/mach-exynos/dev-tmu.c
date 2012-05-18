/* linux/arch/arm/mach-exynos/dev-tmu.c
 *
 * Copyright 2011 by SAMSUNG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/platform_data/exynos4_tmu.h>
#include <asm/irq.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <plat/devs.h>

static struct resource exynos4_tmu_resource[] = {
	[0] = {
		.start	= EXYNOS4_PA_TMU,
		.end	= EXYNOS4_PA_TMU + 0xFFFF - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= EXYNOS4_IRQ_TMU_TRIG0,
		.end	= EXYNOS4_IRQ_TMU_TRIG0,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device exynos4_device_tmu = {
	.name		= "exynos4-tmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos4_tmu_resource),
	.resource	= exynos4_tmu_resource,
};
