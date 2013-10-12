/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS5420 - PLL support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/cpu.h>
#include "board-universal5420.h"

static int exynos5_mfc_clock_init(void)
{
	struct clk *clk_child;
	struct clk *clk_parent;

	clk_child = clk_get(NULL, "aclk_333_sw");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "aclk_333_sw");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "aclk_333_dout");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "aclk_333_dout");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
			"mout_cpll", "aclk_333_pre");
		return PTR_ERR(clk_child);
	}

	clk_set_rate(clk_parent, 333000000);

	clk_put(clk_child);
	clk_put(clk_parent);

	clk_child = clk_get(NULL, "aclk_333");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "aclk_333");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "aclk_333_sw");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "aclk_333_sw");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
			"aclk_333_sw", "aclk_333");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

static int exynos5_mmc_clock_init(void)
{
	struct clk *mout_cpll, *mout_spll, *dw_mmc0, *dw_mmc1, *dw_mmc2;
	int ret = 0;

	/* For eMMC / SD */
	mout_cpll = clk_get(NULL, "mout_cpll");
	if (IS_ERR(mout_cpll)) {
		pr_err("failed to get %s clock\n", "sclk_cpll");
		ret = PTR_ERR(mout_cpll);
		goto err1;
	}

	/* For SDIO */
	mout_spll = clk_get(NULL, "mout_spll");
	if (IS_ERR(mout_spll)) {
		pr_err("failed to get %s clock\n", "sclk_spll");
		ret = PTR_ERR(mout_spll);
		goto err2;
	}

	dw_mmc0 = clk_get_sys("dw_mmc.0", "sclk_dwmci");
	if (IS_ERR(dw_mmc0)) {
		pr_err("%s: %s clock not found\n", "dw_mmc.0", "sclk_dwmci");
		ret = PTR_ERR(dw_mmc0);
		goto err3;
	}

	dw_mmc1 = clk_get_sys("dw_mmc.1", "sclk_dwmci");
	if (IS_ERR(dw_mmc1)) {
		pr_err("%s: %s clock not found\n", "dw_mmc.1", "sclk_dwmci");
		ret = PTR_ERR(dw_mmc1);
		goto err4;
	}

	dw_mmc2 = clk_get_sys("dw_mmc.2", "sclk_dwmci");
	if (IS_ERR(dw_mmc2)) {
		pr_err("%s: %s clock not found\n", "dw_mmc.2", "sclk_dwmci");
		ret = PTR_ERR(dw_mmc2);
		goto err5;
	}

	ret = clk_set_parent(dw_mmc0, mout_cpll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_cpll->name, dw_mmc0->name);
		goto err6;
	}

	ret = clk_set_parent(dw_mmc1, mout_spll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_spll->name, dw_mmc1->name);
		goto err6;
	}

	ret = clk_set_parent(dw_mmc2, mout_cpll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_cpll->name, dw_mmc2->name);
		goto err6;
	}

	clk_set_rate(dw_mmc0, 666 * MHZ);
	clk_set_rate(dw_mmc1, 400 * MHZ);
	clk_set_rate(dw_mmc2, 666 * MHZ);

err6:
	clk_put(dw_mmc2);
err5:
	clk_put(dw_mmc1);
err4:
	clk_put(dw_mmc0);
err3:
	clk_put(mout_spll);
err2:
	clk_put(mout_cpll);
err1:
	return ret;
}

static int exynos5_uart_clock_init(void)
{
	struct clk *uart0, *uart1, *uart2, *uart3;
	struct clk *mout_mpll;
	int ret = 0;

	uart0 = clk_get_sys("s5pv210-uart.0", "uclk1");
	if (IS_ERR(uart0)) {
		pr_err("%s clock not found\n", "s5pv210-uart.0");
		ret = PTR_ERR(uart0);
		goto err0;
	}

	uart1 = clk_get_sys("s5pv210-uart.1", "uclk1");
	if (IS_ERR(uart1)) {
		pr_err("%s clock not found\n", "s5pv210-uart.1");
		ret = PTR_ERR(uart1);
		goto err1;
	}

	uart2 = clk_get_sys("s5pv210-uart.2", "uclk1");
	if (IS_ERR(uart2)) {
		pr_err("%s clock not found\n", "s5pv210-uart.2");
		ret = PTR_ERR(uart2);
		goto err2;
	}

	uart3 = clk_get_sys("s5pv210-uart.3", "uclk1");
	if (IS_ERR(uart3)) {
		pr_err("%s clock not found\n", "s5pv210-uart.3");
		ret = PTR_ERR(uart3);
		goto err3;
	}

	mout_mpll = clk_get(NULL, "mout_mpll");
	if (IS_ERR(mout_mpll)) {
		pr_err("%s clock not found\n", "mout_mpll");
		ret = PTR_ERR(mout_mpll);
		goto err4;
	}


	if (clk_set_parent(uart0, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart0->name);
	else
		clk_set_rate(uart0, 200 * MHZ);

	if (clk_set_parent(uart1, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart1->name);
	else
		clk_set_rate(uart1, 200 * MHZ);

	if (clk_set_parent(uart2, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart2->name);
	else
		clk_set_rate(uart2, 200 * MHZ);

	if (clk_set_parent(uart3, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart3->name);
	else
		clk_set_rate(uart3, 200 * MHZ);

	clk_put(mout_mpll);
err4:
	clk_put(uart3);
err3:
	clk_put(uart2);
err2:
	clk_put(uart1);
err1:
	clk_put(uart0);
err0:
	return ret;
}

static int exynos5_aclk_300_disp1_init(void)
{
	struct clk *aclk_300_disp1 = NULL;
	struct clk *aclk_300_disp1_sw = NULL;
	struct clk *dout_disp1 = NULL;
	struct clk *mout_dpll = NULL;
	int ret;

	aclk_300_disp1 = clk_get(NULL, "aclk_300_disp1");
	if (IS_ERR(aclk_300_disp1)) {
		return PTR_ERR(aclk_300_disp1);
	}

	aclk_300_disp1_sw = clk_get(NULL, "aclk_300_disp1_sw");
	if (IS_ERR(aclk_300_disp1_sw)) {
		pr_err("failed to get aclk_300_disp1_sw\n");
		goto err_clk1;
	}

	dout_disp1 = clk_get(NULL, "aclk_300_disp1_dout");
	if (IS_ERR(dout_disp1)) {
		pr_err("failed to get aclk_300_disp1_dout\n");
		goto err_clk2;
	}

	ret = clk_set_parent(aclk_300_disp1, aclk_300_disp1_sw);
	if (ret < 0) {
		pr_err("failed to clk_set_parent for aclk_300_disp1\n");
		goto err_clk3;
	}

	ret = clk_set_parent(aclk_300_disp1_sw, dout_disp1);
	if (ret < 0) {
		pr_err("failed to clk_set_parent for aclk_300_disp1_sw\n");
		goto err_clk3;
	}

	mout_dpll = clk_get(NULL, "mout_dpll");
	if (IS_ERR(mout_dpll)) {
		pr_err("failed to get mout_dpll for aclk_300_disp1\n");
		goto err_clk3;
	}

	ret = clk_set_parent(dout_disp1, mout_dpll);
	if (ret < 0) {
		pr_err("failed to clk_set_parent for aclk_300_disp1_dout)\n");
		goto err_clk4;
	}

	ret = clk_set_rate(dout_disp1, 300*1000*1000);
	if (ret < 0) {
		pr_err("failed to clk_set_rate for aclk_300_disp1_dout\n");
		goto err_clk4;
	}

	clk_put(aclk_300_disp1);
	clk_put(aclk_300_disp1_sw);
	clk_put(dout_disp1);
	clk_put(mout_dpll);
	return 0;

err_clk4:
	clk_put(mout_dpll);
err_clk3:
	clk_put(dout_disp1);
err_clk2:
	clk_put(aclk_300_disp1_sw);
err_clk1:
	clk_put(aclk_300_disp1);

	return -EINVAL;
}

static int exynos5_aclk_200_disp1_init(void)
{
	struct clk *aclk_200_disp1 = NULL;
	struct clk *aclk_200_sw = NULL;
	struct clk *aclk_200_dout = NULL;
	struct clk *mout_dpll = NULL;
	int ret;

	aclk_200_disp1 = clk_get(NULL, "aclk_200_disp1");
	if (IS_ERR(aclk_200_disp1)) {
		pr_err("failed to get %s clock\n", "aclk_200_disp1");
		return PTR_ERR(aclk_200_disp1);
	}

	aclk_200_sw = clk_get(NULL, "aclk_200_sw");
	if (IS_ERR(aclk_200_sw)) {
		clk_put(aclk_200_disp1);
		pr_err("failed to get %s clock\n", "aclk_200_sw");
		return PTR_ERR(aclk_200_sw);
	}

	ret = clk_set_parent(aclk_200_disp1, aclk_200_sw);
	if (ret < 0) {
		clk_put(aclk_200_disp1);
		clk_put(aclk_200_sw);
		pr_err("Unable to set parent %s of clock %s.\n",
			"aclk_200_disp1", "aclk_200_sw");
		return PTR_ERR(aclk_200_disp1);
	}

	clk_put(aclk_200_disp1);

	aclk_200_dout = clk_get(NULL, "aclk_200_dout");
	if (IS_ERR(aclk_200_dout)) {
		clk_put(aclk_200_sw);
		pr_err("failed to get %s clock\n", "aclk_200_dout");
		return PTR_ERR(aclk_200_dout);
	}

	ret = clk_set_parent(aclk_200_sw, aclk_200_dout);
	if (ret < 0) {
		clk_put(aclk_200_sw);
		clk_put(aclk_200_dout);
		pr_err("Unable to set parent %s of clock %s.\n",
			"aclk_200_sw", "aclk_200_dout");
		return PTR_ERR(aclk_200_sw);
	}

	clk_put(aclk_200_sw);

	mout_dpll = clk_get(NULL, "mout_dpll");
	if (IS_ERR(mout_dpll)) {
		clk_put(aclk_200_dout);
		pr_err("failed to get %s clock\n", "mout_dpll");
		return PTR_ERR(aclk_200_dout);
	}

	ret = clk_set_parent(aclk_200_dout, mout_dpll);
	if (ret < 0) {
		clk_put(aclk_200_dout);
		clk_put(mout_dpll);
		pr_err("Unable to set parent %s of clock %s.\n",
			"aclk_200_dout", "mout_dpll");
		return PTR_ERR(aclk_200_dout);
	}

	clk_put(mout_dpll);

	ret = clk_set_rate(aclk_200_dout, 200*1000*1000);
	if (ret < 0) {
		clk_put(aclk_200_dout);
		pr_err("failed to clk_set_rate for aclk_200_dout\n");
		return PTR_ERR(aclk_200_dout);
	}

	clk_put(aclk_200_dout);

	return 0;
}

static int exynos5_spi_clock_init(void)
{
	struct clk *child_clk = NULL;
	struct clk *parent_clk = NULL;
	char clk_name[16];
	int i;

	parent_clk = clk_get(NULL, "mout_dpll");
	if (IS_ERR(parent_clk)) {
		pr_err("Failed to get mout_dpll clk\n");
		return PTR_ERR(parent_clk);
	}

	for (i = 0; i < 3; i++) {
		snprintf(clk_name, sizeof(clk_name), "sclk_spi%d", i);

		child_clk = clk_get(NULL, clk_name);
		if (IS_ERR(child_clk)) {
			pr_err("Failed to get %s clk\n", clk_name);
			goto err_clk0;
		}

		if (clk_set_parent(child_clk, parent_clk)) {
			pr_err("Unable to set parent %s of clock %s\n",
					parent_clk->name, child_clk->name);
			goto err_clk1;
		}

		clk_set_rate(child_clk, 100 * 1000 * 1000);

		clk_put(child_clk);
	}
	clk_put(parent_clk);
	return 0;

err_clk1:
	clk_put(child_clk);
err_clk0:
	clk_put(parent_clk);
	return PTR_ERR(child_clk);
}

static int exynos5420_mscl_clock_init(void)
{
	struct clk *mout_cpll, *aclk_400_mscl_dout;
	struct clk *aclk_400_mscl_sw, *aclk_400_mscl;
	int ret = 0;

	mout_cpll = clk_get(NULL, "mout_cpll");
	if (IS_ERR(mout_cpll)) {
		pr_err("failed to get %s clock\n", "mout_cpll");
		return PTR_ERR(mout_cpll);
	}

	aclk_400_mscl_dout = clk_get(NULL, "aclk_400_mscl_dout");
	if (IS_ERR(aclk_400_mscl_dout)) {
		pr_err("failed to get %s clock\n", "aclk_400_mscl_dout");
		ret = PTR_ERR(aclk_400_mscl_dout);
		goto err_cpll;
	}

	aclk_400_mscl_sw = clk_get(NULL, "aclk_400_mscl_sw");
	if (IS_ERR(aclk_400_mscl_sw)) {
		pr_err("failed to get %s clock\n", "aclk_400_mscl_sw");
		ret = PTR_ERR(aclk_400_mscl_sw);
		goto err_mscl_dout;
	}

	aclk_400_mscl = clk_get(NULL, "aclk_400_mscl");
	if (IS_ERR(aclk_400_mscl)) {
		pr_err("failed to get %s clock\n", "aclk_400_mscl");
		ret = PTR_ERR(aclk_400_mscl);
		goto err_mscl_sw;
	}

	if (clk_set_parent(aclk_400_mscl_dout, mout_cpll))
		pr_err("Unable to set parent %s of clock %s\n",
			mout_cpll->name, aclk_400_mscl_dout->name);

	if (clk_set_parent(aclk_400_mscl_sw, aclk_400_mscl_dout))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_400_mscl_dout->name, aclk_400_mscl_sw->name);

	if (clk_set_parent(aclk_400_mscl, aclk_400_mscl_sw))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_400_mscl_sw->name, aclk_400_mscl->name);

	if (clk_set_rate(aclk_400_mscl_dout, 400 * MHZ))
		pr_err("Can't set %s clock rate\n", aclk_400_mscl_dout->name);

	clk_put(aclk_400_mscl);
err_mscl_sw:
	clk_put(aclk_400_mscl_sw);
err_mscl_dout:
	clk_put(aclk_400_mscl_dout);
err_cpll:
	clk_put(mout_cpll);

	return ret;
}

static int exynos5420_gscl_clock_init(void)
{
	struct clk *mout_dpll, *aclk_300_gscl_dout;
	struct clk *aclk_300_gscl_sw, *aclk_300_gscl;
	int ret = 0;

	mout_dpll = clk_get(NULL, "mout_dpll");
	if (IS_ERR(mout_dpll)) {
		pr_err("failed to get %s clock\n", "mout_dpll");
		return PTR_ERR(mout_dpll);
	}

	aclk_300_gscl_dout = clk_get(NULL, "aclk_300_gscl_dout");
	if (IS_ERR(aclk_300_gscl_dout)) {
		pr_err("failed to get %s clock\n", "aclk_300_gscl_dout");
		ret = PTR_ERR(aclk_300_gscl_dout);
		goto err_dpll;
	}

	aclk_300_gscl_sw = clk_get(NULL, "aclk_300_gscl_sw");
	if (IS_ERR(aclk_300_gscl_sw)) {
		pr_err("failed to get %s clock\n", "aclk_300_gscl_sw");
		ret = PTR_ERR(aclk_300_gscl_sw);
		goto err_gscl_dout;
	}

	aclk_300_gscl = clk_get(NULL, "aclk_300_gscl");
	if (IS_ERR(aclk_300_gscl)) {
		pr_err("failed to get %s clock\n", "aclk_300_gscl");
		ret = PTR_ERR(aclk_300_gscl);
		goto err_gscl_sw;
	}

	if (clk_set_parent(aclk_300_gscl_dout, mout_dpll))
		pr_err("Unable to set parent %s of clock %s\n",
			mout_dpll->name, aclk_300_gscl_dout->name);

	if (clk_set_parent(aclk_300_gscl_sw, aclk_300_gscl_dout))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_300_gscl_dout->name, aclk_300_gscl_sw->name);

	if (clk_set_parent(aclk_300_gscl, aclk_300_gscl_sw))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_300_gscl_sw->name, aclk_300_gscl->name);

	if (clk_set_rate(aclk_300_gscl_dout, 300 * MHZ))
		pr_err("Can't set %s clock rate\n", aclk_300_gscl_dout->name);

	clk_put(aclk_300_gscl);
err_gscl_sw:
	clk_put(aclk_300_gscl_sw);
err_gscl_dout:
	clk_put(aclk_300_gscl_dout);
err_dpll:
	clk_put(mout_dpll);

	return ret;
}

static int exynos5420_g2d_clock_init(void)
{
	/* CPLL:666MHz */
	struct clk *mout_cpll, *aclk_333_g2d_dout;
	struct clk *aclk_333_g2d_sw, *aclk_333_g2d;
	int ret = 0;

	mout_cpll = clk_get(NULL, "mout_cpll");
	if (IS_ERR(mout_cpll)) {
		pr_err("failed to get %s clock\n", "mout_mpll");
		return PTR_ERR(mout_cpll);
	}

	aclk_333_g2d_dout = clk_get(NULL, "aclk_333_g2d_dout");
	if (IS_ERR(aclk_333_g2d_dout)) {
		pr_err("failed to get %s clock\n", "aclk_333_g2d_dout");
		ret = PTR_ERR(aclk_333_g2d_dout);
		goto err_mpll;
	}

	aclk_333_g2d_sw = clk_get(NULL, "aclk_333_g2d_sw");
	if (IS_ERR(aclk_333_g2d_sw)) {
		pr_err("failed to get %s clock\n", "aclk_333_g2d_sw");
		ret = PTR_ERR(aclk_333_g2d_sw);
		goto err_g2d_dout;
	}

	aclk_333_g2d = clk_get(NULL, "aclk_333_g2d"); /* sclk_fimg2d */
	if (IS_ERR(aclk_333_g2d)) {
		pr_err("failed to get %s clock\n", "aclk_333_g2d");
		ret = PTR_ERR(aclk_333_g2d);
		goto err_g2d_sw;
	}

	if (clk_set_parent(aclk_333_g2d_dout, mout_cpll))
		pr_err("Unable to set parent %s of clock %s\n",
			mout_cpll->name, aclk_333_g2d_dout->name);

	if (clk_set_parent(aclk_333_g2d_sw, aclk_333_g2d_dout))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_333_g2d_dout->name, aclk_333_g2d_sw->name);

	if (clk_set_parent(aclk_333_g2d, aclk_333_g2d_sw))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_333_g2d_sw->name, aclk_333_g2d->name);

	/* CLKDIV_ACLK_333_G2D */
	if (clk_set_rate(aclk_333_g2d_dout, 333 * MHZ))
		pr_err("Can't set %s clock rate\n", aclk_333_g2d_dout->name);

	clk_put(aclk_333_g2d);
err_g2d_sw:
	clk_put(aclk_333_g2d_sw);
err_g2d_dout:
	clk_put(aclk_333_g2d_dout);
err_mpll:
	clk_put(mout_cpll);

	return ret;
}

static int exynos5420_jpeg_clock_init(void)
{
	struct clk *clk_child;
	struct clk *clk_parent;

	clk_child = clk_get(NULL, "aclk_300_jpeg_sw");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "aclk_300_jpeg_sw");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "aclk_300_jpeg_dout");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "aclk_300_jpeg_dout");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);

		pr_err("Unable to set parent %s of clock %s.\n",
				"mout_dpll", "aclk_300_jpeg_sw");
		return PTR_ERR(clk_child);
	}

	clk_set_rate(clk_parent, 300000000);

	clk_put(clk_child);
	clk_put(clk_parent);

	clk_child = clk_get(NULL, "aclk_300_jpeg_dout");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "aclk_300_jpeg_dout");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_dpll");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "mout_dpll");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
				"mout_dpll", "aclk_300_jpeg_sw");
		return PTR_ERR(clk_child);
	}

	clk_child = clk_get(NULL, "aclk_300_jpeg");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "aclk_300_jpeg");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "aclk_300_jpeg_sw");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "aclk_300_jpeg_sw");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
				"aclk_300_jpeg_sw", "aclk_300_jpeg");
		return PTR_ERR(clk_child);
	}

	pr_debug("set jpeg aclk rate %lu\n", clk_get_rate(clk_child));

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

static int exynos5420_set_parent(char *child_name, char *parent_name)
{
	struct clk *clk_child;
	struct clk *clk_parent;

	clk_child = clk_get(NULL, child_name);
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", child_name);
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, parent_name);
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", parent_name);
		return PTR_ERR(clk_parent);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);

		pr_err("Unable to set parent %s of clock %s.\n",
				parent_name, child_name);
		return -EINVAL;
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

static int exynos5420_top_clock_init(void)
{
	struct clk *clk_target;
	int retval = 0;

	if (exynos5420_set_parent("pclk_200_fsys_dout", "mout_dpll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_dpll", "pclk_200_fsys_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_100_noc_dout", "mout_dpll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_mpll", "aclk_100_noc_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_400_wcore_dout", "aclk_400_wcore_mout")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "aclk_400_wcore_mout", "aclk_400_wcore_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_400_wcore_sw", "mout_spll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_spll", "aclk_400_wcore_sw");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_266_dout", "mout_mpll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_mpll", "aclk_266_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_66_dout", "mout_cpll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_cpll", "aclk_66_dout");
		retval = -EINVAL;
	}
	clk_target = clk_get(NULL, "aclk_66_dout");
	clk_set_rate(clk_target, 67000000);
	clk_put(clk_target);

	if (exynos5420_set_parent("aclk_333_432_isp0", "aclk_333_432_isp0_sw")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "aclk_333_432_isp0_sw", "aclk_333_432_isp0");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_333_432_isp_dout", "mout_ipll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_ipll", "aclk_333_432_isp_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_266_g2d_dout", "mout_mpll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_mpll", "aclk_266_g2d_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_400_disp1_dout", "mout_dpll")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "mout_dpll", "aclk_400_disp1_dout");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("aclk_400_disp1", "aclk_400_disp1_sw")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "aclk_400_disp1_sw", "aclk_400_disp1");
		retval = -EINVAL;
	}

	if (exynos5420_set_parent("mout_rpll", "ext_xtal")) {
		pr_err("%s failed to set parent %s of clock %s.\n"
				, __func__, "ext_xtal", "mout_rpll");
		retval = -EINVAL;
	} else {
		clk_target = clk_get(NULL, "fout_rpll");
		/* To sync initial state */
		clk_enable(clk_target);
		clk_disable(clk_target);
		clk_put(clk_target);
	}

	return retval;
}

void __init exynos5_universal5420_clock_init(void)
{
	if (exynos5_mfc_clock_init())
		pr_err("failed to MFC clock init\n");

	if (exynos5_uart_clock_init())
		pr_err("failed to init uart clock init\n");

	if (exynos5_mmc_clock_init())
		pr_err("failed to init emmc clock init\n");

	if (exynos5_aclk_300_disp1_init())
		pr_err("failed to init aclk_300_disp1\n");

	if (exynos5_aclk_200_disp1_init())
		pr_err("failed to init aclk_200_disp1\n");

	if (exynos5_spi_clock_init())
		pr_err("failed to init spi clock init\n");

	if (exynos5420_mscl_clock_init())
		pr_err("failed to init mscl clock init\n");

	if (exynos5420_gscl_clock_init())
		pr_err("failed to init gscl clock init\n");

	if (exynos5420_g2d_clock_init())
		pr_err("failed to init g2d clock init\n");

	if (exynos5420_jpeg_clock_init())
		pr_err("failed to init jpeg clock init\n");

	if (exynos5420_top_clock_init())
		pr_err("failed to init top clock init\n");
}
