/*
 * linux/arch/arm/plat-s5p/dev-sss.c
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * Base S5P Crypto Engine resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/export.h>

#include <mach/map.h>
#include <mach/irqs.h>

#include <asm/sizes.h>

static struct resource s5p_sss_resource[] = {
	[0] = {
		.start	= S5P_PA_SSS,
		.end	= S5P_PA_SSS + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
#if defined(CONFIG_ARCH_S5PV210)
	[1] = {
		.start	= IRQ_SSS_INT,
		.end	= IRQ_SSS_HASH,
		.flags	= IORESOURCE_IRQ,
	},
#elif defined(CONFIG_ARCH_EXYNOS4) || defined(CONFIG_ARCH_EXYNOS5)
	[1] = {
		.start	= IRQ_INTFEEDCTRL_SSS,
		.end	= IRQ_INTFEEDCTRL_SSS,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device s5p_device_sss = {
	.name		= "s5p-sss",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s5p_sss_resource),
	.resource	= s5p_sss_resource,
};
EXPORT_SYMBOL(s5p_device_sss);

static struct resource s5p_slimsss_resource[] = {
	[0] = DEFINE_RES_MEM(S5P_PA_SLIMSSS, SZ_4K),
	[1] = DEFINE_RES_IRQ(IRQ_INTFEEDCTRL_SLIMSSS),
};

struct platform_device s5p_device_slimsss = {
	.name		= "s5p-slimsss",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s5p_slimsss_resource),
	.resource	= s5p_slimsss_resource,
};
EXPORT_SYMBOL(s5p_device_slimsss);
