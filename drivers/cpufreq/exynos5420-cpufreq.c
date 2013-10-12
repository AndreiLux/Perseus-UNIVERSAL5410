/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5420 - CPU frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>

#include <plat/clock.h>

#define CPUFREQ_LEVEL_END_CA7	(L14 + 1)
#define CPUFREQ_LEVEL_END_CA15	(L22 + 1)

#undef PRINT_DIV_VAL

#define ENABLE_CLKOUT
#define SUPPORT_APLL_BYPASS

static int max_support_idx_CA7;
static int max_support_idx_CA15;
static int min_support_idx_CA7 = (CPUFREQ_LEVEL_END_CA7 - 1);
static int min_support_idx_CA15 = (CPUFREQ_LEVEL_END_CA15 - 1);

static struct clk *mout_cpu;
static struct clk *mout_kfc;
static struct clk *mout_hpm_cpu;
static struct clk *mout_hpm_kfc;
static struct clk *mout_apll;
static struct clk *mout_kpll;
static struct clk *fout_apll;
static struct clk *fout_kpll;
static struct clk *mx_mspll_cpu;
static struct clk *mx_mspll_kfc;
static struct clk *fout_spll;

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv;
	unsigned int	clkdiv1;
};

static unsigned int exynos5420_volt_table_CA7[CPUFREQ_LEVEL_END_CA7];
static unsigned int exynos5420_volt_table_CA15[CPUFREQ_LEVEL_END_CA15];
struct pm_qos_request exynos5_cpu_mif_qos;

static struct cpufreq_frequency_table exynos5420_freq_table_CA7[] = {
	{L0, 1600 * 1000},
	{L1, 1500 * 1000},
	{L2, 1400 * 1000},
	{L3, 1300 * 1000},
	{L4, 1200 * 1000},
	{L5, 1100 * 1000},
	{L6, 1000 * 1000},
	{L7,  900 * 1000},
	{L8,  800 * 1000},
	{L9,  700 * 1000},
	{L10, 600 * 1000},
	{L11, 500 * 1000},
	{L12, 400 * 1000},
	{L13, 300 * 1000},
	{L14, 200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_frequency_table exynos5420_freq_table_CA15[] = {
	{L0,  2400 * 1000},
	{L1,  2300 * 1000},
	{L2,  2200 * 1000},
	{L3,  2100 * 1000},
	{L4,  2000 * 1000},
	{L5,  1900 * 1000},
	{L6,  1800 * 1000},
	{L7,  1700 * 1000},
	{L8,  1600 * 1000},
	{L9,  1500 * 1000},
	{L10, 1400 * 1000},
	{L11, 1300 * 1000},
	{L12, 1200 * 1000},
	{L13, 1100 * 1000},
	{L14, 1000 * 1000},
	{L15,  900 * 1000},
	{L16,  800 * 1000},
	{L17,  700 * 1000},
	{L18,  600 * 1000},
	{L19,  500 * 1000},
	{L20,  400 * 1000},
	{L21,  300 * 1000},
	{L22,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5420_clkdiv_table_CA7[CPUFREQ_LEVEL_END_CA7];
static struct cpufreq_clkdiv exynos5420_clkdiv_table_CA15[CPUFREQ_LEVEL_END_CA15];

static unsigned int clkdiv_cpu0_5420_CA7[CPUFREQ_LEVEL_END_CA7][5] = {
	/*
	 * Clock divider value for following
	 * { KFC, ACLK, HPM, PCLK, KPLL }
	 */

	/* ARM L0: 1.6GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L1: 1.5GMHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L2: 1.4GMHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L3: 1.3GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L4: 1.2GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L5: 1.1GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L6: 1000MHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L7: 900MHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L8: 800MHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L9: 700MHz */
	{ 0, 2, 7, 4, 3 },

	/* ARM L10: 600MHz */
	{ 0, 2, 7, 4, 3 },

	/* ARM L11: 500MHz */
	{ 0, 2, 7, 4, 3 },

	/* ARM L12: 400MHz */
	{ 0, 2, 7, 3, 3 },

	/* ARM L13: 300MHz */
	{ 0, 2, 7, 3, 3 },

	/* ARM L14: 200MHz */
	{ 0, 2, 7, 3, 3 },
};

static unsigned int clkdiv_cpu0_5420_CA15[CPUFREQ_LEVEL_END_CA15][7] = {
	/*
	 * Clock divider value for following
	 * { CPUD, ATB, PCLK_DBG, APLL, ARM2}
	 */

	/* ARM L0: 2.4GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L1: 2.3GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L2: 2.2GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L3: 2.1GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L4: 2.0GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L5: 1.9GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L6: 1.8GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L7: 1.7GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L8: 1.6GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L9: 1.5GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L10: 1.4GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L11: 1.3GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L12: 1.2GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L13: 1.1GHz */
	{ 2, 7, 7, 3, 0 },

	/* ARM L14: 1000MHz */
	{ 2, 6, 6, 3, 0 },

	/* ARM L15: 900MHz */
	{ 2, 6, 6, 3, 0 },

	/* ARM L16: 800MHz */
	{ 2, 5, 5, 3, 0 },

	/* ARM L17: 700MHz */
	{ 2, 5, 5, 3, 0 },

	/* ARM L18: 600MHz */
	{ 2, 4, 4, 3, 0 },

	/* ARM L19: 500MHz */
	{ 2, 3, 3, 3, 0 },

	/* ARM L20: 400MHz */
	{ 2, 3, 3, 3, 0 },

	/* ARM L21: 300MHz */
	{ 2, 3, 3, 3, 0 },

	/* ARM L22: 200MHz */
	{ 2, 3, 3, 3, 0 },
};

unsigned int clkdiv_cpu1_5420_CA15[CPUFREQ_LEVEL_END_CA15][2] = {
	/*
	 * Clock divider value for following
	 * { copy, HPM }
	 */

	/* ARM L0: 2.4GHz */
	{ 7, 7 },

	/* ARM L1: 2.3GHz */
	{ 7, 7 },

	/* ARM L2: 2.2GHz */
	{ 7, 7 },

	/* ARM L3: 2.1GHz */
	{ 7, 7 },

	/* ARM L4: 2.0GHz */
	{ 7, 7 },

	/* ARM L5: 1.9GHz */
	{ 7, 7 },

	/* ARM L6: 1.8GHz */
	{ 7, 7 },

	/* ARM L7: 1.7GHz */
	{ 7, 7 },

	/* ARM L8: 1.6GHz */
	{ 7, 7 },

	/* ARM L9: 1.5GHz */
	{ 7, 7 },

	/* ARM L10: 1.4GHz */
	{ 7, 7 },

	/* ARM L11: 1.3GHz */
	{ 7, 7 },

	/* ARM L12: 1.2GHz */
	{ 7, 7 },

	/* ARM L13: 1.1GHz */
	{ 7, 7 },

	/* ARM L14: 1000MHz */
	{ 7, 7 },

	/* ARM L15: 900MHz */
	{ 7, 7 },

	/* ARM L16: 800MHz */
	{ 7, 7 },

	/* ARM L17: 700MHz */
	{ 7, 7 },

	/* ARM L18: 600MHz */
	{ 7, 7 },

	/* ARM L19: 500MHz */
	{ 7, 7 },

	/* ARM L20: 400MHz */
	{ 7, 7 },

	/* ARM L21: 300MHz */
	{ 7, 7 },

	/* ARM L22: 200MHz */
	{ 7, 7 },
};

static unsigned int exynos5420_kpll_pms_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	/* KPLL FOUT L0: 1.6GHz */
	((200 << 16) | (3 << 8) | (0x0)),

	/* KPLL FOUT L1: 1.5GHz */
	((250 << 16) | (4 << 8) | (0x0)),

	/* KPLL FOUT L2: 1.4GHz */
	((175 << 16) | (3 << 8) | (0x0)),

	/* KPLL FOUT L3: 1.3GHz */
	((325 << 16) | (6 << 8) | (0x0)),

	/* KPLL FOUT L4: 1.2GHz */
	((200 << 16) | (2 << 8) | (0x1)),

	/* KPLL FOUT L5: 1.1GHz */
	((275 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L6: 1000MHz */
	((250 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L7: 900MHz */
	((150 << 16) | (2 << 8) | (0x1)),

	/* KPLL FOUT L8: 800MHz */
	((200 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L9: 700MHz */
	((175 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L10: 600MHz */
	((100 << 16) | (2 << 8) | (0x1)),

	/* KPLL FOUT L11: 500MHz */
	((250 << 16) | (3 << 8) | (0x2)),

	/* KPLL FOUT L12: 400MHz */
	((200 << 16) | (3 << 8) | (0x2)),

	/* KPLL FOUT L13: 300MHz */
	((100 << 16) | (2 << 8) | (0x2)),

	/* KPLL FOUT L14: 200MHz */
	((200 << 16) | (3 << 8) | (0x3)),
};

static unsigned int exynos5420_apll_pms_table_CA15[CPUFREQ_LEVEL_END_CA15] = {
	/* APLL FOUT L0: 2.4GHz */
	((200 << 16) | (2 << 8) | (0x0)),

	/* APLL FOUT L1: 2.3GHz */
	((575 << 16) | (6 << 8) | (0x0)),

	/* APLL FOUT L2: 2.2GHz */
	((275 << 16) | (3 << 8) | (0x0)),

	/* APLL FOUT L3: 2.1GHz */
	((175 << 16) | (2 << 8) | (0x0)),

	/* APLL FOUT L4: 2.0GHz */
	((250 << 16) | (3 << 8) | (0x0)),

	/* APLL FOUT L5: 1.9GHz */
	((475 << 16) | (6 << 8) | (0x0)),

	/* APLL FOUT L6: 1.8GHz */
	((225 << 16) | (3 << 8) | (0x0)),

	/* APLL FOUT L7: 1.7GHz */
	((425 << 16) | (6 << 8) | (0x0)),

	/* APLL FOUT L8: 1.6GHz */
	((200 << 16) | (3 << 8) | (0x0)),

	/* APLL FOUT L9: 1.5GHz */
	((250 << 16) | (4 << 8) | (0x0)),

	/* APLL FOUT L10: 1.4GHz */
	((175 << 16) | (3 << 8) | (0x0)),

	/* APLL FOUT L11: 1.3GHz */
	((325 << 16) | (6 << 8) | (0x0)),

	/* APLL FOUT L12: 1.2GHz */
	((200 << 16) | (2 << 8) | (0x1)),

	/* APLL FOUT L13: 1.1GHz */
	((275 << 16) | (3 << 8) | (0x1)),

	/* APLL FOUT L14: 1000MHz */
	((250 << 16) | (3 << 8) | (0x1)),

	/* APLL FOUT L15: 900MHz */
	((150 << 16) | (2 << 8) | (0x1)),

	/* APLL FOUT L16: 800MHz */
	((200 << 16) | (3 << 8) | (0x1)),

	/* APLL FOUT L17: 700MHz */
	((175 << 16) | (3 << 8) | (0x1)),

	/* APLL FOUT L18: 600MHz */
	((100 << 16) | (2 << 8) | (0x1)),

	/* APLL FOUT L19: 500MHz */
	((250 << 16) | (3 << 8) | (0x2)),

	/* APLL FOUT L20: 400MHz */
	((200 << 16) | (3 << 8) | (0x2)),

	/* APLL FOUT L21: 300MHz */
	((100 << 16) | (2 << 8) | (0x2)),

	/* APLL FOUT L22: 200MHz */
	((200 << 16) | (3 << 8) | (0x3)),
};

/*
 * ASV group voltage table
 */

static const unsigned int asv_voltage_5420_CA7[CPUFREQ_LEVEL_END_CA7] = {
	1200000,	/* LO 1600 */
	1200000,	/* L1 1500 */
	1200000,	/* L2 1400 */
	1200000,	/* L3 1300 */
	1200000,	/* L4 1200 */
	1200000,	/* L5 1100 */
	1100000,	/* L6 1000 */
	1100000,	/* L7  900 */
	1100000,	/* L8  800 */
	1000000,	/* L9  700 */
	1000000,	/* L10 600 */
	1000000,	/* L11 500 */
	1000000,	/* L12 400 */
	 900000,	/* L13 300 */
	 900000,	/* L14 200 */
};

static const unsigned int asv_voltage_5420_CA15[CPUFREQ_LEVEL_END_CA15] = {
	1200000,	/* L0  2400 */
	1200000,	/* L1  2300 */
	1200000,	/* L2  2200 */
	1200000,	/* L3  2100 */
	1200000,	/* L4  2000 */
	1200000,	/* L5  1900 */
	1200000,	/* L6  1800 */
	1200000,	/* L7  1700 */
	1200000,	/* L8  1600 */
	1100000,	/* L9  1500 */
	1100000,	/* L10  1400 */
	1100000,	/* L11 1300 */
	1000000,	/* L12 1200 */
	1000000,	/* L13 1100 */
	1000000,	/* L14 1000 */
	1000000,	/* L15  900 */
	 900000,	/* L16  800 */
	 900000,	/* L17  700 */
	 900000,	/* L18  600 */
	 900000,	/* L19  500 */
	 900000,	/* L20  400 */
	 900000,	/* L22  300 */
	 900000,	/* L22  200 */
};

/*
 * This frequency value is selected as a max dvfs level depends
 * on the number of big cluster's working cpus
 * If one big cpu is working and other cpus are LITTLE, big cpu
 * can go to max_op_freq_b[0] frequency
 */
static const unsigned int exynos5420_max_op_freq_b_evt0[NR_CPUS + 1] = {
	UINT_MAX,
#ifdef CONFIG_EXYNOS5_MAX_CPU_HOTPLUG
	2100000,
	2100000,
	2100000,
	2100000,
#else
	1900000,
	1900000,
	1900000,
	1900000,
#endif
};

/* Minimum memory throughput in megabytes per second */
static int exynos5420_bus_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	266000,	/* 1.6 GHz */
	266000,	/* 1.5 GHz */
	266000,	/* 1.4 GHz */
	266000,	/* 1.3 GHz */
	266000,	/* 1.2 GHz */
	266000,	/* 1.1 GHz */
	266000,	/* 1.0 GHz */
	160000,	/* 900 MHz */
	160000,	/* 800 MHz */
	160000,	/* 700 MHz */
	133000,	/* 600 MHz */
	133000,	/* 500 MHz */
	0,	/* 400 MHz */
	0,	/* 300 MHz */
	0,	/* 200 MHz */
};

static int exynos5420_bus_table_CA15[CPUFREQ_LEVEL_END_CA15] = {
	800000,	/* 2.4 GHz */
	800000,	/* 2.3 GHz */
	800000,	/* 2.2 GHz */
	800000,	/* 2.1 GHz */
	800000,	/* 2.0 GHz */
	733000,	/* 1.9 GHz */
	733000,	/* 1.8 GHz */
	733000,	/* 1.7 MHz */
	733000,	/* 1.6 GHz */
	667000,	/* 1.5 GHz */
	667000,	/* 1.4 GHz */
	667000,	/* 1.3 GHz */
	667000,	/* 1.2 GHz */
	533000,	/* 1.1 GHz */
	533000,	/* 1.0 GHz */
	400000,	/* 900 MHz */
	400000,	/* 800 MHz */
	400000,	/* 700 MHz */
	400000,	/* 600 MHz */
	400000,	/* 500 MHz */
	400000,	/* 400 MHz */
	400000,	/* 300 MHz */
	400000,	/* 200 MHz */
};

static void exynos5420_set_ema_CA15(unsigned int target_volt)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS54XX_ARM_EMA_CTRL);

	if ((target_volt <= EXYNOS54XX_ARM_EMA_BASE_VOLT) &&
			(tmp & ~EXYNOS54XX_ARM_WAS_ENABLE)) {
		tmp |= EXYNOS54XX_ARM_WAS_ENABLE;
		__raw_writel(tmp, EXYNOS54XX_ARM_EMA_CTRL);
	} else if ((target_volt > EXYNOS54XX_ARM_EMA_BASE_VOLT) &&
			(tmp & EXYNOS54XX_ARM_WAS_ENABLE)) {
		tmp &= ~EXYNOS54XX_ARM_WAS_ENABLE;
		__raw_writel(tmp, EXYNOS54XX_ARM_EMA_CTRL);
	}
};

static void exynos5420_set_clkdiv_CA7(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - KFC0 */

	tmp = exynos5420_clkdiv_table_CA7[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS5_CLKDIV_KFC0);

	while (__raw_readl(EXYNOS5_CLKDIV_STAT_KFC0) & 0x11111111)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5_CLKDIV_KFC0);
	pr_info("DIV_KFC0[0x%x]\n", tmp);
#endif
}

static void exynos5420_set_clkdiv_CA15(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - CPU0 */

	tmp = exynos5420_clkdiv_table_CA15[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS5_CLKDIV_CPU0);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_CLKDIV_STATCPU0);
	} while (tmp & 0x11111111);

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5_CLKDIV_CPU0);
	pr_info("DIV_CPU0[0x%x]\n", tmp);
#endif

	/* Change Divider - CPU1 */
	tmp = exynos5420_clkdiv_table_CA15[div_index].clkdiv1;

	__raw_writel(tmp, EXYNOS5_CLKDIV_CPU1);

	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_CLKDIV_STATCPU1);
	} while (tmp & 0x111);

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5_CLKDIV_CPU1);
	pr_info("DIV_CPU1[0x%x]\n", tmp);
#endif
}

static void exynos5420_set_kpll_CA7(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	clk_enable(fout_spll);
	/* 0. before change to MPLL, set div for MPLL output */
	if ((new_index < L5) && (old_index < L5))
		exynos5420_set_clkdiv_CA7(L5); /* pll_safe_index of CA7 */

	/* 1. CLKMUX_CPU_KFC = MX_MSPLL_KFC, KFCCLK uses MX_MSPLL_KFC for lock time */
	if (clk_set_parent(mout_kfc, mx_mspll_kfc))
		pr_err("Unable to set parent %s of clock %s.\n",
				mx_mspll_kfc->name, mout_kfc->name);
	/* 1.1 CLKMUX_HPM_KFC = MX_MSPLL_KFC, SCLKHPM uses MX_MSPLL_KFC for lock time */
	if (clk_set_parent(mout_hpm_kfc, mx_mspll_kfc))
		pr_err("Unable to set parent %s of clock %s.\n",
				mx_mspll_kfc->name, mout_hpm_kfc->name);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS5_CLKMUX_STAT_KFC)
			>> EXYNOS5_CLKSRC_KFC_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set KPLL Lock time */
	pdiv = ((exynos5420_kpll_pms_table_CA7[new_index] >> 8) & 0x3f);

	__raw_writel((pdiv * 250), EXYNOS5_KPLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5_KPLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos5420_kpll_pms_table_CA7[new_index];
	__raw_writel(tmp, EXYNOS5_KPLL_CON0);

	/* 4. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_KPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5_KPLLCON0_LOCKED_SHIFT)));

	/* 5. CLKMUX_CPU_KFC = KPLL */
	if (clk_set_parent(mout_kfc, mout_kpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_kpll->name, mout_kfc->name);
	/* 5.1 CLKMUX_HPM_KFC = KPLL */
	if (clk_set_parent(mout_hpm_kfc, mout_kpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_kpll->name, mout_hpm_kfc->name);

	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_CLKMUX_STAT_KFC);
		tmp &= EXYNOS5_CLKMUX_STATKFC_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS5_CLKSRC_KFC_MUXCORE_SHIFT));

	/* 6. restore original div value */
	if ((new_index < L5) && (old_index < L5))
		exynos5420_set_clkdiv_CA7(new_index);

	clk_disable(fout_spll);
}

static void exynos5420_set_apll_CA15(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	clk_enable(fout_spll);
	/* 1. CLKMUX_CPU = MX_MSPLL_CPU, ARMCLK uses MX_MSPLL_CPU for lock time */
	if (clk_set_parent(mout_cpu, mx_mspll_cpu))
		pr_err(KERN_ERR "Unable to set parent %s of clock %s.\n",
			mx_mspll_cpu->name, mout_cpu->name);
	/* 1.1 CLKMUX_HPM = MX_MSPLL_CPU, CLKHPM uses MX_MSPLL_KFC for lock time */
	if (clk_set_parent(mout_hpm_cpu, mx_mspll_cpu))
		pr_err(KERN_ERR "Unable to set parent %s of clock %s.\n",
			mx_mspll_cpu->name, mout_hpm_cpu->name);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS5_CLKMUX_STATCPU)
			>> EXYNOS5_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set APLL Lock time */
	pdiv = ((exynos5420_apll_pms_table_CA15[new_index] >> 8) & 0x3f);

	__raw_writel((pdiv * 250), EXYNOS5_APLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5_APLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos5420_apll_pms_table_CA15[new_index];
	 __raw_writel(tmp, EXYNOS5_APLL_CON0);

	/* 4. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_APLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5_APLLCON0_LOCKED_SHIFT)));

	/* 5. MUX_CORE_SEL = APLL */
	if (clk_set_parent(mout_cpu, mout_apll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_apll->name, mout_cpu->name);
	/* 5..1 CLKMUX_HPM = APLL */
	if (clk_set_parent(mout_hpm_cpu, mout_apll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_apll->name, mout_hpm_cpu->name);

	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_CLKMUX_STATCPU);
		tmp &= EXYNOS5_CLKMUX_STATCPU_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS5_CLKSRC_CPU_MUXCORE_SHIFT));
	clk_disable(fout_spll);
}

static bool exynos5420_pms_change_CA7(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos5420_kpll_pms_table_CA7[old_index] >> 8);
	unsigned int new_pm = (exynos5420_kpll_pms_table_CA7[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}

static bool exynos5420_pms_change_CA15(unsigned int old_index,
				       unsigned int new_index)
{
	unsigned int old_pm = (exynos5420_apll_pms_table_CA15[old_index] >> 8);
	unsigned int new_pm = (exynos5420_apll_pms_table_CA15[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5420_set_frequency_CA7(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5420_pms_change_CA7(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5420_set_clkdiv_CA7(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_KPLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5420_kpll_pms_table_CA7[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_KPLL_CON0);

		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5420_set_clkdiv_CA7(new_index);
			/* 2. Change the apll m,p,s value */
			exynos5420_set_kpll_CA7(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5420_pms_change_CA7(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_KPLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5420_kpll_pms_table_CA7[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_KPLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5420_set_clkdiv_CA7(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the apll m,p,s value */
			exynos5420_set_kpll_CA7(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5420_set_clkdiv_CA7(new_index);
		}
	}

	if (old_index >= new_index) {
		if (pm_qos_request_active(&exynos5_cpu_mif_qos))
			pm_qos_update_request(&exynos5_cpu_mif_qos,
					exynos5420_bus_table_CA7[new_index]);
	}

	clk_set_rate(fout_kpll, exynos5420_freq_table_CA7[new_index].frequency * 1000);

	if (old_index < new_index) {
		if (pm_qos_request_active(&exynos5_cpu_mif_qos))
			pm_qos_update_request(&exynos5_cpu_mif_qos,
					exynos5420_bus_table_CA7[new_index]);
	}
}

static void exynos5420_set_frequency_CA15(unsigned int old_index,
					  unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5420_pms_change_CA15(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5420_set_clkdiv_CA15(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5420_apll_pms_table_CA15[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_APLL_CON0);

		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5420_set_clkdiv_CA15(new_index);
			/* 2. Change the apll m,p,s value */
			exynos5420_set_apll_CA15(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5420_pms_change_CA15(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5420_apll_pms_table_CA15[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_APLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5420_set_clkdiv_CA15(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the apll m,p,s value */
			exynos5420_set_apll_CA15(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5420_set_clkdiv_CA15(new_index);
		}
	}

	if (old_index >= new_index) {
		if (pm_qos_request_active(&exynos5_cpu_mif_qos))
			pm_qos_update_request(&exynos5_cpu_mif_qos,
					exynos5420_bus_table_CA15[new_index]);
	}

	clk_set_rate(fout_apll, exynos5420_freq_table_CA15[new_index].frequency * 1000);

	if (old_index < new_index) {
		if (pm_qos_request_active(&exynos5_cpu_mif_qos))
			pm_qos_update_request(&exynos5_cpu_mif_qos,
					exynos5420_bus_table_CA15[new_index]);
	}
}

static void __init set_volt_table_CA7(void)
{
	unsigned int i;
	unsigned int asv_volt __maybe_unused;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		/* FIXME: need to update voltage table for REV1 */
		asv_volt = get_match_volt(ID_KFC, exynos5420_freq_table_CA7[i].frequency);

		if (!asv_volt)
			exynos5420_volt_table_CA7[i] = asv_voltage_5420_CA7[i];
		else
			exynos5420_volt_table_CA7[i] = asv_volt;

		pr_info("CPUFREQ of CA7  L%d : %d uV\n", i,
				exynos5420_volt_table_CA7[i]);
	}

	exynos5420_freq_table_CA7[L0].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA7[L1].frequency = CPUFREQ_ENTRY_INVALID;
#if defined(CONFIG_V1A) || defined(CONFIG_N1A)
	exynos5420_freq_table_CA7[L2].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA7[L3].frequency = CPUFREQ_ENTRY_INVALID;
	max_support_idx_CA7 = L4;
#else
	exynos5420_freq_table_CA7[L2].frequency = CPUFREQ_ENTRY_INVALID;
	max_support_idx_CA7 = L3;
#endif

	min_support_idx_CA7 = L11;
	exynos5420_freq_table_CA7[L12].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA7[L13].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA7[L14].frequency = CPUFREQ_ENTRY_INVALID;
}

static void __init set_volt_table_CA15(void)
{
	unsigned int i;
	unsigned int asv_volt __maybe_unused;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA15; i++) {
		/* FIXME: need to update voltage table for REV1 */
		asv_volt = get_match_volt(ID_ARM, exynos5420_freq_table_CA15[i].frequency);

		if (!asv_volt)
			exynos5420_volt_table_CA15[i] = asv_voltage_5420_CA15[i];
		else
			exynos5420_volt_table_CA15[i] = asv_volt;

		pr_info("CPUFREQ of CA15 L%d : %d uV\n", i,
				exynos5420_volt_table_CA15[i]);
	}

	exynos5420_freq_table_CA15[L0].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L1].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L2].frequency = CPUFREQ_ENTRY_INVALID;
#ifdef CONFIG_EXYNOS5_MAX_CPU_HOTPLUG
	max_support_idx_CA15 = L3;
#else
	exynos5420_freq_table_CA15[L3].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L4].frequency = CPUFREQ_ENTRY_INVALID;
#if defined(CONFIG_V1A) || defined(CONFIG_N1A)
	exynos5420_freq_table_CA15[L5].frequency = CPUFREQ_ENTRY_INVALID;
	max_support_idx_CA15 = L6;
#else
	max_support_idx_CA15 = L5;
#endif
#endif

	min_support_idx_CA15 = L16;
	exynos5420_freq_table_CA15[L17].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L18].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L19].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L20].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L21].frequency = CPUFREQ_ENTRY_INVALID;
	exynos5420_freq_table_CA15[L22].frequency = CPUFREQ_ENTRY_INVALID;
}

int __init exynos5_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;
	struct clk *sclk_spll;

	set_volt_table_CA7();

	mout_kfc = clk_get(NULL, "mout_kfc");
	if (IS_ERR(mout_kfc))
		return PTR_ERR(mout_kfc);

	mout_hpm_kfc = clk_get(NULL, "mout_hpm_kfc");
	if (IS_ERR(mout_hpm_kfc))
		goto err_mout_hpm_kfc;

	mx_mspll_kfc = clk_get(NULL, "mx_mspll_kfc");
	if (IS_ERR(mx_mspll_kfc))
		goto err_mx_mspll_kfc;
	sclk_spll = clk_get(NULL, "mout_spll");
	clk_set_parent(mx_mspll_kfc, sclk_spll);
	clk_put(sclk_spll);

	fout_spll = clk_get(NULL, "fout_spll");
	if (IS_ERR(fout_spll))
		goto err_fout_spll;

	rate = clk_get_rate(mx_mspll_kfc) / 1000;

	mout_kpll = clk_get(NULL, "mout_kpll");
	if (IS_ERR(mout_kpll))
		goto err_mout_kpll;

	fout_kpll = clk_get(NULL, "fout_kpll");
	if (IS_ERR(fout_kpll))
		goto err_fout_kpll;

	for (i = L0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		exynos5420_clkdiv_table_CA7[i].index = i;

		tmp = __raw_readl(EXYNOS5_CLKDIV_KFC0);

		tmp &= ~(EXYNOS5_CLKDIV_KFC0_CORE_MASK |
			EXYNOS5_CLKDIV_KFC0_ACLK_MASK |
			EXYNOS5_CLKDIV_KFC0_HPM_MASK |
			EXYNOS5_CLKDIV_KFC0_PCLK_MASK |
			EXYNOS5_CLKDIV_KFC0_KPLL_MASK);

		tmp |= ((clkdiv_cpu0_5420_CA7[i][0] << EXYNOS5_CLKDIV_KFC0_CORE_SHIFT) |
			(clkdiv_cpu0_5420_CA7[i][1] << EXYNOS5_CLKDIV_KFC0_ACLK_SHIFT) |
			(clkdiv_cpu0_5420_CA7[i][2] << EXYNOS5_CLKDIV_KFC0_HPM_SHIFT) |
			(clkdiv_cpu0_5420_CA7[i][3] << EXYNOS5_CLKDIV_KFC0_PCLK_SHIFT)|
			(clkdiv_cpu0_5420_CA7[i][4] << EXYNOS5_CLKDIV_KFC0_KPLL_SHIFT));

		exynos5420_clkdiv_table_CA7[i].clkdiv = tmp;
	}

	info->mpll_freq_khz = rate;
	info->pm_lock_idx = L0;
	info->pll_safe_idx = L5;
	info->max_support_idx = max_support_idx_CA7;
	info->min_support_idx = min_support_idx_CA7;
	info->cpu_clk = mout_kfc;

	info->max_op_freqs = exynos5420_max_op_freq_b_evt0;

	info->volt_table = exynos5420_volt_table_CA7;
	info->freq_table = exynos5420_freq_table_CA7;
	info->set_freq = exynos5420_set_frequency_CA7;
	info->need_apll_change = exynos5420_pms_change_CA7;

#ifdef ENABLE_CLKOUT
	tmp = __raw_readl(EXYNOS5_CLKOUT_CMU_KFC);
	tmp &= ~0xffff;
	tmp |= 0x1904;
	__raw_writel(tmp, EXYNOS5_CLKOUT_CMU_KFC);
#endif

	return 0;

err_fout_kpll:
	clk_put(fout_kpll);
err_mout_kpll:
	clk_put(mout_kpll);
err_fout_spll:
	clk_put(fout_spll);
err_mx_mspll_kfc:
	clk_put(mx_mspll_kfc);
err_mout_hpm_kfc:
	clk_put(mout_kfc);
	clk_put(mout_hpm_kfc);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}

int __init exynos5_cpufreq_CA15_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;
	struct clk *sclk_spll;

	set_volt_table_CA15();

	mout_cpu = clk_get(NULL, "mout_cpu");
	if (IS_ERR(mout_cpu))
		return PTR_ERR(mout_cpu);

	mout_hpm_cpu = clk_get(NULL, "mout_hpm_cpu");
	if (IS_ERR(mout_hpm_cpu))
		goto err_mout_hpm_cpu;

	mx_mspll_cpu = clk_get(NULL, "mx_mspll_cpu");
	if (IS_ERR(mx_mspll_cpu))
		goto err_mx_mspll_cpu;
	sclk_spll = clk_get(NULL, "mout_spll");
	clk_set_parent(mx_mspll_cpu, sclk_spll);
	clk_put(sclk_spll);

	rate = clk_get_rate(mx_mspll_cpu) / 1000;

	mout_apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(mout_apll))
		goto err_mout_apll;

	fout_apll = clk_get(NULL, "fout_apll");
	if (IS_ERR(fout_apll))
		goto err_fout_apll;

	for (i = L0; i < CPUFREQ_LEVEL_END_CA15; i++) {
		exynos5420_clkdiv_table_CA15[i].index = i;

		tmp = __raw_readl(EXYNOS5_CLKDIV_CPU0);

		tmp &= ~(EXYNOS5_CLKDIV_CPU0_CPUD_MASK |
			EXYNOS5_CLKDIV_CPU0_ATB_MASK |
			EXYNOS5_CLKDIV_CPU0_PCLKDBG_MASK |
			EXYNOS5_CLKDIV_CPU0_APLL_MASK |
			EXYNOS5_CLKDIV_CPU0_CORE2_MASK);

		tmp |= ((clkdiv_cpu0_5420_CA15[i][0] << EXYNOS5_CLKDIV_CPU0_CPUD_SHIFT) |
			(clkdiv_cpu0_5420_CA15[i][1] << EXYNOS5_CLKDIV_CPU0_ATB_SHIFT) |
			(clkdiv_cpu0_5420_CA15[i][2] << EXYNOS5_CLKDIV_CPU0_PCLKDBG_SHIFT) |
			(clkdiv_cpu0_5420_CA15[i][3] << EXYNOS5_CLKDIV_CPU0_APLL_SHIFT) |
			(clkdiv_cpu0_5420_CA15[i][4] << EXYNOS5_CLKDIV_CPU0_CORE2_SHIFT));

		exynos5420_clkdiv_table_CA15[i].clkdiv = tmp;

		tmp = __raw_readl(EXYNOS5_CLKDIV_CPU1);

		tmp &= ~(EXYNOS5_CLKDIV_CPU1_COPY_MASK |
			EXYNOS5_CLKDIV_CPU1_HPM_MASK);
		tmp |= ((clkdiv_cpu1_5420_CA15[i][0] << EXYNOS5_CLKDIV_CPU1_COPY_SHIFT) |
			(clkdiv_cpu1_5420_CA15[i][1] << EXYNOS5_CLKDIV_CPU1_HPM_SHIFT));

		exynos5420_clkdiv_table_CA15[i].clkdiv1 = tmp;
	}

	info->mpll_freq_khz = rate;
	info->pm_lock_idx = L0;
	info->pll_safe_idx = L12;
	info->max_support_idx = max_support_idx_CA15;
	info->min_support_idx = min_support_idx_CA15;
#ifdef SUPPORT_APLL_BYPASS
	info->cpu_clk = fout_apll;
	pr_info("fout_apll[%lu]\n", clk_get_rate(fout_apll));
#else
	info->cpu_clk = mout_cpu;
#endif
	info->max_op_freqs = exynos5420_max_op_freq_b_evt0;

	info->volt_table = exynos5420_volt_table_CA15;
	info->freq_table = exynos5420_freq_table_CA15;
	info->set_freq = exynos5420_set_frequency_CA15;
	//info->set_ema = exynos5420_set_ema_CA15;
	info->need_apll_change = exynos5420_pms_change_CA15;

#ifdef ENABLE_CLKOUT
	tmp = __raw_readl(EXYNOS5_CLKOUT_CMU_CPU);
	tmp &= ~0xffff;
	tmp |= 0x1904;
	__raw_writel(tmp, EXYNOS5_CLKOUT_CMU_CPU);
#endif

	return 0;

err_fout_apll:
	clk_put(fout_apll);
err_mout_apll:
	clk_put(mout_apll);
err_mx_mspll_cpu:
	clk_put(mx_mspll_cpu);
err_mout_hpm_cpu:
	clk_put(mout_hpm_cpu);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
