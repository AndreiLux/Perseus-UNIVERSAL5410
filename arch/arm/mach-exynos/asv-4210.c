/* linux/arch/arm/mach-exynos/asv-4210.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4210 - ASV(Adaptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <plat/clock.h>

#include <mach/regs-iem.h>
#include <mach/regs-clock.h>
#include <mach/asv.h>

/*
 * exynos_result_of_asv is result of ASV group.
 * Using by this value, other driver can adjust voltage.
 */
unsigned int exynos_result_of_asv;

enum target_asv {
	EXYNOS4210_1200,
	EXYNOS4210_1400,
	EXYNOS4210_SINGLE_1200,
};

struct asv_judge_table exynos4210_1200_limit[] = {
	/* HPM , IDS */
	{8 , 4},
	{11 , 8},
	{14 , 12},
	{18 , 17},
	{21 , 27},
	{23 , 45},
	{25 , 55},
};

static struct asv_judge_table exynos4210_1400_limit[] = {
	/* HPM , IDS */
	{13 , 8},
	{17 , 12},
	{22 , 32},
	{26 , 52},
};

static struct asv_judge_table exynos4210_single_1200_limit[] = {
	/* HPM , IDS */
	{8 , 4},
	{14 , 12},
	{21 , 27},
	{25 , 55},
};

static int exynos4210_asv_pre_clock_init(void)
{
	struct clk *clk_hpm;
	struct clk *clk_copy;
	struct clk *clk_parent;

	/* PWI clock setting */
	clk_copy = clk_get(NULL, "sclk_pwi");
	if (IS_ERR(clk_copy))
		goto clock_fail;
	else {
		clk_parent = clk_get(NULL, "xusbxti");

		if (IS_ERR(clk_parent)) {
			clk_put(clk_copy);

			goto clock_fail;
		}
		if (clk_set_parent(clk_copy, clk_parent))
			goto clock_fail;

		clk_put(clk_parent);
	}
	clk_set_rate(clk_copy, (48 * MHZ));

	clk_put(clk_copy);

	/* HPM clock setting */
	clk_copy = clk_get(NULL, "dout_copy");
	if (IS_ERR(clk_copy))
		goto clock_fail;
	else {
		clk_parent = clk_get(NULL, "mout_mpll");
		if (IS_ERR(clk_parent)) {
			clk_put(clk_copy);

			goto clock_fail;
		}
		if (clk_set_parent(clk_copy, clk_parent))
			goto clock_fail;

		clk_put(clk_parent);
	}

	clk_set_rate(clk_copy, (400 * MHZ));

	clk_put(clk_copy);

	clk_hpm = clk_get(NULL, "sclk_hpm");
	if (IS_ERR(clk_hpm))
		goto clock_fail;

	clk_set_rate(clk_hpm, (200 * MHZ));

	clk_put(clk_hpm);

	return 0;

clock_fail:
	pr_err("EXYNOS4210: ASV: Clock init fail\n");

	return -EBUSY;
}

static int exynos4210_asv_pre_clock_setup(void)
{
	/* APLL_CON0 level register */
	__raw_writel(0x80FA0601, EXYNOS4_APLL_CON0L8);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L7);
	__raw_writel(0x80C80602, EXYNOS4_APLL_CON0L6);
	__raw_writel(0x80C80604, EXYNOS4_APLL_CON0L5);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L4);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L3);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L2);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L1);

	/* IEM Divider register */
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L8);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L7);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L6);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L5);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L4);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L3);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L2);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L1);

	return 0;
}

static int exynos4210_find_group(struct samsung_asv *asv_info,
			      enum target_asv exynos4_target)
{
	unsigned int ret = 0;
	unsigned int i;

	if (exynos4_target == EXYNOS4210_1200) {
		ret = ARRAY_SIZE(exynos4210_1200_limit);

		for (i = 0; i < ARRAY_SIZE(exynos4210_1200_limit); i++) {
			if (asv_info->hpm_result <= exynos4210_1200_limit[i].hpm_limit ||
			   asv_info->ids_result <= exynos4210_1200_limit[i].ids_limit) {
				ret = i;
				break;
			}
		}
	} else if (exynos4_target == EXYNOS4210_1400) {
		ret = ARRAY_SIZE(exynos4210_1400_limit);

		for (i = 0; i < ARRAY_SIZE(exynos4210_1400_limit); i++) {
			if (asv_info->hpm_result <= exynos4210_1400_limit[i].hpm_limit ||
			   asv_info->ids_result <= exynos4210_1400_limit[i].ids_limit) {
				ret = i;
				break;
			}
		}
	} else if (exynos4_target == EXYNOS4210_SINGLE_1200) {
		ret = ARRAY_SIZE(exynos4210_single_1200_limit);

		for (i = 0; i < ARRAY_SIZE(exynos4210_single_1200_limit); i++) {
			if (asv_info->hpm_result <= exynos4210_single_1200_limit[i].hpm_limit ||
			   asv_info->ids_result <= exynos4210_single_1200_limit[i].ids_limit) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

#define PACK_ID				8
#define PACK_MASK			0x3

#define SUPPORT_1400MHZ			(1 << 31)
#define SUPPORT_1200MHZ			(1 << 30)
#define SUPPORT_1000MHZ			(1 << 29)

static int exynos4210_get_hpm(struct samsung_asv *asv_info)
{
	unsigned int i;
	unsigned int tmp;
	unsigned int hpm_delay = 0;
	void __iomem *iem_base;

	iem_base = ioremap(EXYNOS4_PA_IEM, SZ_128K);

	if (!iem_base) {
		pr_err("EXYNOS4210: ASV: ioremap fail\n");
		return -EPERM;
	}

	/* Clock setting to get asv value */
	if (!asv_info->pre_clock_init)
		goto err;
	else {
		if (asv_info->pre_clock_init())
			goto err;
		else {
			/* HPM enable  */
			tmp = __raw_readl(iem_base + EXYNOS4_APC_CONTROL);
			tmp |= APC_HPM_EN;
			__raw_writel(tmp, (iem_base + EXYNOS4_APC_CONTROL));

			asv_info->pre_clock_setup();

			/* IEM enable */
			tmp = __raw_readl(iem_base + EXYNOS4_IECDPCCR);
			tmp |= IEC_EN;
			__raw_writel(tmp, (iem_base + EXYNOS4_IECDPCCR));
		}
	}

	/* Get HPM Delay value */
	for (i = 0; i < EXYNOS4_LOOP_CNT; i++) {
		tmp = __raw_readb(iem_base + EXYNOS4_APC_DBG_DLYCODE);
		hpm_delay += tmp;
	}

	hpm_delay /= EXYNOS4_LOOP_CNT;

	/* Store result of hpm value */
	asv_info->hpm_result = hpm_delay;

	return 0;

err:
	pr_err("EXYNOS4210: ASV: Failt to get hpm function\n");

	iounmap(iem_base);

	return -EPERM;
}

static int exynos4210_get_ids(struct samsung_asv *asv_info)
{
	unsigned int pkg_id_val;

	if (!asv_info->ids_offset || !asv_info->ids_mask) {
		pr_err("EXYNOS4210: ASV: No ids_offset or No ids_mask\n");

		return -EPERM;
	}

	pkg_id_val = __raw_readl(S5P_VA_CHIPID + 0x4);
	asv_info->pkg_id = pkg_id_val;
	asv_info->ids_result = ((pkg_id_val >> asv_info->ids_offset) &
							asv_info->ids_mask);

	return 0;
}

static int exynos4210_asv_store_result(struct samsung_asv *asv_info)
{
	unsigned int result_grp;
	char *support_freq;
	unsigned int exynos_idcode = 0x0;

	exynos_result_of_asv = 0;

	exynos_idcode = __raw_readl(S5P_VA_CHIPID);

	/* Single chip is only support 1.2GHz */
	if (!((exynos_idcode >> PACK_ID) & PACK_MASK)) {
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_SINGLE_1200);
		result_grp |= SUPPORT_1200MHZ;
		support_freq = "1.2GHz";

		goto set_reg;
	}

	/* Check support freq */
	switch (asv_info->pkg_id & 0x7) {
	/* Support 1.2GHz */
	case 1:
	case 7:
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_1200);
		result_grp |= SUPPORT_1200MHZ;
		support_freq = "1.2GHz";
		break;
	/* Support 1.4GHz */
	case 5:
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_1400);
		result_grp |= SUPPORT_1200MHZ;
		support_freq = "1.4GHz";
		break;
	/* Defalut support 1.0GHz */
	default:
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_1200);
		result_grp |= SUPPORT_1000MHZ;
		support_freq = "1.0GHz";
		break;
	}

set_reg:
	exynos_result_of_asv = result_grp;

	pr_info("EXYNOS4: ASV: Support %s and Group is 0x%x\n",
					support_freq, result_grp);

	return 0;
}

void exynos4210_asv_init(struct samsung_asv *asv_info)
{
	pr_info("EXYNOS4210: Adaptive Support Voltage init\n");

	asv_info->ids_offset = 24;
	asv_info->ids_mask = 0xFF;

	asv_info->get_ids = exynos4210_get_ids;
	asv_info->get_hpm = exynos4210_get_hpm;
	asv_info->pre_clock_init = exynos4210_asv_pre_clock_init;
	asv_info->pre_clock_setup = exynos4210_asv_pre_clock_setup;
	asv_info->store_result = exynos4210_asv_store_result;
}
