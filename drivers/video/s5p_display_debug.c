/*
 * Exynos Display Debug support
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>

void print_reg_pm_disp_5410(void)
{
        printk("1. PMU\n");
        printk("DISP1_STATUS           [0x%x]\n", __raw_readl(EXYNOS5410_DISP1_STATUS));
        printk("MIPI_DPHY_CONTORL0     [0x%x]\n", __raw_readl(S5P_MIPI_DPHY_CONTROL(0)));
        printk("MIPI_DPHY_CONTORL1     [0x%x]\n", __raw_readl(S5P_MIPI_DPHY_CONTROL(1)));

        printk("2. CMU\n");
        printk("CLK_SRC_DISP10         [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_DISP1_0));
        printk("CLK_SRC_TOP0           [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_TOP0));
        printk("CLK_SRC_TOP1           [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_TOP1));
        printk("CLK_SRC_TOP2           [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_TOP2));
        printk("CLK_SRC_TOP3           [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_TOP3));
        printk("CLK_SRC_MASK_DISP00    [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_DISP0_0));
        printk("CLK_SRC_MASK_DISP01    [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_DISP0_1));
        printk("CLK_SRC_MASK_DISP10    [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_DISP1_0));
        printk("CLK_SRC_MASK_DISP11    [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_DISP1_1));

        printk("CLK_DIV_TOP0           [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_TOP0));
        printk("CLK_DIV_TOP1           [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_TOP1));
        printk("CLK_DIV_TOP2           [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_TOP2));
        printk("CLK_DIV_TOP3           [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_TOP3));
        printk("CLK_DIV_DISP00         [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_DISP0_0));
        printk("CLK_DIV_DISP00         [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_DISP0_1));
        printk("CLK_DIV_DISP10         [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_DISP1_0));
        printk("CLK_DIV_DISP11         [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_DISP1_1));
        printk("CLK_DIV2_RATIO0        [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV2_RATIO0));
        printk("CLK_DIV2_RATIO1        [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV2_RATIO1));

        printk("CLK_GATE_BUS_DISP0     [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BUS_DISP0));
        printk("CLK_GATE_BUS_DISP1     [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BUS_DISP1));
        printk("CLK_GATE_TOP_SCLK_DISP0[0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_TOP_SCLK_DISP0));
        printk("CLK_GATE_TOP_SCLK_DISP1[0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_TOP_SCLK_DISP1));
        printk("CLK_GATE_IP_DISP0      [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_IP_DISP0));
        printk("CLK_GATE_IP_DISP1      [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_IP_DISP1));
        printk("CLK_GATE_BLOCK         [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BLOCK));
        printk("End ...\n");
}
