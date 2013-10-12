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
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <mach/map.h>
#include <mach/exynos-scaler.h>

static void *exynos5_scaler_setup_clocks(void)
{
	struct clk **pclk;
	int ret;

	/*
	 * array structure:
	 * [leaf clock, parent, ... , grand parent, NULL]
	 */
	pclk = kmalloc(sizeof(*pclk) * 3, GFP_KERNEL);
	if (!pclk)
		return ERR_PTR(-ENOMEM);

	pclk[0] = clk_get(NULL, "aclk_400_mscl");
	if (IS_ERR(pclk[0])) {
		pr_err("Failed to get 'aclk_400_mscl'\n");
		ret = PTR_ERR(pclk[0]);
		goto err_clk0;
	}

	pclk[1] = clk_get(NULL, "aclk_400_mscl_sw");
	if (IS_ERR(pclk[1])) {
		pr_err("Failed to get 'aclk_400_mscl_sw'\n");
		ret = PTR_ERR(pclk[1]);
		goto err_clk1;
	}

	pclk[2] = NULL;

	return pclk;

err_clk1:
	clk_put(pclk[0]);
err_clk0:
	kfree(pclk);
	return ERR_PTR(ret);
}

static bool exynos5_scaler_init_clocks(void *p)
{
	struct clk **pclk = p;

	if (!p)
		return true;

	while (*pclk && *(pclk + 1)) {
		if (clk_set_parent(*pclk, *(pclk + 1))) {
			pr_err("Unable to set parent %s of clock %s\n",
				(*pclk)->name, (*(pclk + 1))->name);
			return false;
		}
		pclk++;
	}

	return true;
}

static void exynos5_scaler_clean_clocks(void *p)
{
	struct clk **pclk = p;

	while (*pclk)
		clk_put(*pclk++);

	kfree(p);
}

struct exynos_scaler_platdata exynos5410_scaler_pd = {
	.use_pclk	= 1,
	.clk_rate	= 300 * MHZ,
};

struct exynos_scaler_platdata exynos5_scaler_pd = {
	.use_pclk	= 0,
	.clk_rate	= 400 * MHZ,
	.setup_clocks	= exynos5_scaler_setup_clocks,
	.init_clocks	= exynos5_scaler_init_clocks,
	.clean_clocks	= exynos5_scaler_clean_clocks,
};

static struct resource exynos5_scaler0_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_MSCL0, SZ_4K),
	[1] = DEFINE_RES_IRQ(EXYNOS5_IRQ_MSCL0),
};

struct platform_device exynos5_device_scaler0 = {
	.name		= "exynos5-scaler",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(exynos5_scaler0_resource),
	.resource	= exynos5_scaler0_resource,
};

static struct resource exynos5_scaler1_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_MSCL1, SZ_4K),
	[1] = DEFINE_RES_IRQ(EXYNOS5_IRQ_MSCL1),
};

struct platform_device exynos5_device_scaler1 = {
	.name		= "exynos5-scaler",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(exynos5_scaler1_resource),
	.resource	= exynos5_scaler1_resource,
};

static struct resource exynos5_scaler2_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_MSCL2, SZ_4K),
	[1] = DEFINE_RES_IRQ(EXYNOS5_IRQ_MSCL2),
};

struct platform_device exynos5_device_scaler2 = {
	.name		= "exynos5-scaler",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(exynos5_scaler2_resource),
	.resource	= exynos5_scaler2_resource,
};
