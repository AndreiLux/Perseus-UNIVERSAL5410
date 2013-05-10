/*
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
 *
 * Exynos5 series device definition for hs-i2c device 1
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
	[0] = DEFINE_RES_MEM(EXYNOS5250_PA_HSIIC1, SZ_4K),
	[1] = DEFINE_RES_IRQ(IRQ_IIC1),
};

struct platform_device exynos5_device_hs_i2c1 = {
	.name		  = "exynos5-hs-i2c",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(exynos5_hs_i2c_resource),
	.resource	  = exynos5_hs_i2c_resource,
};

void __init exynos5_hs_i2c1_set_platdata(struct exynos5_platform_i2c *pd)
{
	struct exynos5_platform_i2c *npd;

	if (!pd) {
		pd = &default_hs_i2c_data;
		pd->bus_number = 5;
	}

	npd = kmemdup(pd, sizeof(struct exynos5_platform_i2c), GFP_KERNEL);
	if (!npd) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return;
	}

	if (!npd->cfg_gpio)
		npd->cfg_gpio = exynos5_hs_i2c1_cfg_gpio;

	exynos5_device_hs_i2c1.dev.platform_data = npd;
	if (soc_is_exynos5410()) {
		exynos5_device_hs_i2c1.resource[0].start = EXYNOS5410_PA_HSIIC(1);
		exynos5_device_hs_i2c1.resource[0].end = EXYNOS5410_PA_HSIIC(1) + SZ_4K - 1;
		exynos5_device_hs_i2c1.resource[1].start = IRQ_IIC5;
		exynos5_device_hs_i2c1.resource[1].end = IRQ_IIC5;
	}
}
