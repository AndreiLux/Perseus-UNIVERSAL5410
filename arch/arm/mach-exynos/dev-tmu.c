/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/irq.h>

#include <plat/devs.h>
#include <plat/cpu.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/tmu.h>
#include <linux/platform_data/exynos_thermal.h>

static struct resource tmu_resource_5410[] = {
	[0] = DEFINE_RES_MEM(S5P_PA_TMU, SZ_16K),
	[1] = DEFINE_RES_MEM(EXYNOS54XX_PA_TMU1, SZ_16K),
	[2] = DEFINE_RES_MEM(EXYNOS54XX_PA_TMU2, SZ_16K),
	[3] = DEFINE_RES_MEM(EXYNOS54XX_PA_TMU3, SZ_16K),
	[4] = DEFINE_RES_IRQ(EXYNOS5_IRQ_TMU),
	[5] = DEFINE_RES_IRQ(EXYNOS54XX_IRQ_TMU1),
	[6] = DEFINE_RES_IRQ(EXYNOS54XX_IRQ_TMU2),
	[7] = DEFINE_RES_IRQ(EXYNOS54XX_IRQ_TMU3),
};

struct platform_device exynos5410_device_tmu = {
	.name	= "exynos5-tmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tmu_resource_5410),
	.resource	= tmu_resource_5410,
};

static struct resource tmu_resource_5420[] = {
	[0] = DEFINE_RES_MEM(S5P_PA_TMU, SZ_16K),
	[1] = DEFINE_RES_MEM(EXYNOS54XX_PA_TMU1, SZ_16K),
	[2] = DEFINE_RES_MEM(EXYNOS54XX_PA_TMU2, SZ_16K),
	[3] = DEFINE_RES_MEM(EXYNOS54XX_PA_TMU3, SZ_16K),
	[4] = DEFINE_RES_MEM(EXYNOS5420_PA_TMU_GPU, SZ_16K),
	[5] = DEFINE_RES_IRQ(EXYNOS5_IRQ_TMU),
	[6] = DEFINE_RES_IRQ(EXYNOS54XX_IRQ_TMU1),
	[7] = DEFINE_RES_IRQ(EXYNOS54XX_IRQ_TMU2),
	[8] = DEFINE_RES_IRQ(EXYNOS54XX_IRQ_TMU3),
	[9] = DEFINE_RES_IRQ(EXYNOS5420_IRQ_TMU_GPU),
};

struct platform_device exynos5420_device_tmu = {
	.name	= "exynos5-tmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tmu_resource_5420),
	.resource	= tmu_resource_5420,
};

void __init exynos_tmu_set_platdata(struct exynos_tmu_platform_data *pd)
{
	if (soc_is_exynos5410())
		s3c_set_platdata(pd, sizeof(struct exynos_tmu_platform_data),
			&exynos5410_device_tmu);
	else if (soc_is_exynos5420())
		s3c_set_platdata(pd, sizeof(struct exynos_tmu_platform_data),
			&exynos5420_device_tmu);
	else
		pr_err("%s : failed to set TMU platform data\n", __func__);

}
