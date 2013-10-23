/*
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
 *
 * Exynos5 series device definition for hs-i2c device 6
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>

#include <plat/cpu.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/hs-iic.h>

static struct resource exynos5_hs_i2c_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5420_PA_HSIIC6, SZ_4K),
	[1] = DEFINE_RES_IRQ(IRQ_IIC6),
};

struct platform_device exynos5_device_hs_i2c6 = {
	.name		  = "exynos5-hs-i2c",
	.id		  = 6,
	.num_resources	  = ARRAY_SIZE(exynos5_hs_i2c_resource),
	.resource	  = exynos5_hs_i2c_resource,
};

void __init exynos5_hs_i2c6_set_platdata(struct exynos5_platform_i2c *pd)
{
	struct exynos5_platform_i2c *npd;

	if (!pd) {
		pd = &default_hs_i2c_data;
		pd->bus_number = 10;
	}

	npd = kmemdup(pd, sizeof(struct exynos5_platform_i2c), GFP_KERNEL);
	if (!npd) {
		pr_err("%s: no memory for platform data\n", __func__);
		return;
	}

	if (!npd->cfg_gpio)
		npd->cfg_gpio = exynos5_hs_i2c6_cfg_gpio;

	exynos5_device_hs_i2c6.dev.platform_data = npd;
	if (soc_is_exynos5420()) {
		exynos5_device_hs_i2c6.resource[0].start = EXYNOS5420_PA_HSIIC6;
		exynos5_device_hs_i2c6.resource[0].end = EXYNOS5420_PA_HSIIC6 + SZ_4K - 1;
		exynos5_device_hs_i2c6.resource[1].start = IRQ_IIC10;
		exynos5_device_hs_i2c6.resource[1].end = IRQ_IIC10;
	}
}
