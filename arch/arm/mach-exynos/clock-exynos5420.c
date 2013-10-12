/* linux/arch/arm/mach-exynos/clock-exynos5420.c
 *
 * Copyright (c) 2010-2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5420 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>
#include <linux/spinlock.h>
#include <linux/module.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/pm.h>

#include <mach/map.h>
#include <mach/sysmmu.h>
#include <mach/regs-clock.h>

#define clk_fin_rpll clk_ext_xtal_mux
#define clk_fin_spll clk_ext_xtal_mux

struct pll_2550 {
	struct pll_div_data *pms_div;
	int pms_cnt;
	void __iomem *pll_con0;
	void __iomem *pll_lock;
};

struct pll_2650 {
	struct vpll_div_data *pms_div;
	int pms_cnt;
	void __iomem *pll_con0;
	void __iomem *pll_con1;
	unsigned int pllcon1_k_mask;
	void __iomem *pll_lock;
};

#define PLL_2550(plldat, pllname, pmsdiv) \
	static struct pll_2550 plldat = { \
		.pms_div = pmsdiv, \
		.pms_cnt = ARRAY_SIZE(pmsdiv), \
		.pll_con0 = EXYNOS5_ ## pllname ## _CON0, \
		.pll_lock = EXYNOS5_ ## pllname ## _LOCK, }

#define PLL_2650(plldat, pllname, pmsdiv) \
	static struct pll_2650 plldat = { \
		.pms_div = pmsdiv, \
		.pms_cnt = ARRAY_SIZE(pmsdiv), \
		.pll_con0 = EXYNOS5_ ## pllname ## _CON0, \
		.pll_con1 = EXYNOS5_ ## pllname ## _CON1, \
		.pllcon1_k_mask = EXYNOS5_ ## pllname ## CON1_K_MASK, \
		.pll_lock = EXYNOS5_ ## pllname ## _LOCK, }
/*
 * Virtual PLL clock
 */
struct clk clk_v_epll = {
	.name		= "v_epll",
	.id		= -1,
};

struct clk clk_v_vpll = {
	.name		= "v_vpll",
	.id		= -1,
};

struct clk clk_v_ipll = {
	.name		= "v_ipll",
	.id		= -1,
};

/*
 * PLL output clock
 */
struct clk clk_fout_kpll = {
	.name		= "fout_kpll",
	.id		= -1,
};

struct clk clk_fout_rpll = {
	.name		= "fout_rpll",
	.id		= -1,
};

struct clk clk_fout_spll = {
	.name		= "fout_spll",
	.id		= -1,
};

struct clk clk_ipll = {
	.name		= "ipll",
	.id		= -1,
};

/* Bypass output for IPLL */
static struct clk *clk_bypass_ipll_list[] = {
	[0] = &clk_ipll,
	[1] = &clk_fin_ipll,
};

static struct clksrc_sources clk_bypass_ipll = {
	.sources		= clk_bypass_ipll_list,
	.nr_sources		= ARRAY_SIZE(clk_bypass_ipll_list),
};

static struct clksrc_clk exynos5420_clk_fout_ipll = {
	.clk	= {
		.name		= "fout_ipll",
	},
	.sources = &clk_bypass_ipll,
	.reg_src = { .reg = EXYNOS5_IPLL_CON1, .shift = 22, .size = 1 },
};

static unsigned long xtal_rate;

/* This setup_pll function will set rate and set parent the pll */
static void setup_pll(const char *pll_name,
		struct clk *parent_clk, unsigned long rate)
{
	struct clk *tmp_clk;

	clk_set_rate(parent_clk, rate);
	tmp_clk = clk_get(NULL, pll_name);
	clk_set_parent(tmp_clk, parent_clk);
	clk_put(tmp_clk);
}

static void __init exynos5420_pll_init(void)
{
	/* CPLL 666MHz */
	setup_pll("mout_cpll", &clk_fout_cpll, 666000000);
	/* EPLL 100MHz */
	setup_pll("mout_epll", &clk_fout_epll, 100000000);
	clk_set_parent(&clk_fout_epll, &clk_v_epll);
	/* IPLL 432MHz */
	setup_pll("fout_ipll", &clk_ipll, 432000000);
	/* VPLL 350MHz */
	setup_pll("mout_vpll", &clk_fout_vpll, 350000000);
	clk_set_parent(&clk_fout_vpll, &clk_v_vpll);
	/* SPLL 66MHz */
	setup_pll("mout_spll", &clk_fout_spll, 400000000);
	/* MPLL 532MHz */
	setup_pll("mout_mpll", &clk_fout_mpll, 532000000);
	/* RPLL 266MHz */
	setup_pll("mout_rpll", &clk_fout_rpll, 266000000);
	/* DPLL 600MHz */
	setup_pll("fout_dpll", &clk_fout_dpll, 600000000);
}

#ifdef CONFIG_PM_SLEEP
static struct sleep_save exynos5420_clock_save[] = {
	SAVE_ITEM(EXYNOS5_CLKSRC_CPU),
	SAVE_ITEM(EXYNOS5_CLKSRC_CPERI0),
	SAVE_ITEM(EXYNOS5_CLKSRC_CPERI1),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP0),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP1),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP2),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP3),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP4),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP5),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP6),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP7),
	SAVE_ITEM(EXYNOS5_CLKSRC_MAUDIO),
	SAVE_ITEM(EXYNOS5_CLKSRC_CDREX),
	SAVE_ITEM(EXYNOS5_CLKSRC_DISP1_0),
	SAVE_ITEM(EXYNOS5_CLKSRC_DISP1_1),
	SAVE_ITEM(EXYNOS5_CLKSRC_KFC),
	SAVE_ITEM(EXYNOS5_CLKSRC_PERIC0),
	SAVE_ITEM(EXYNOS5_CLKSRC_PERIC1),
	SAVE_ITEM(EXYNOS5_CLKSRC_GSCL),
	SAVE_ITEM(EXYNOS5_CLKDIV_GEN),
	SAVE_ITEM(EXYNOS5_CLKDIV_CDREX),
	SAVE_ITEM(EXYNOS5_CLKDIV_CDREX2),
	SAVE_ITEM(EXYNOS5_CLKDIV_ACP),
	SAVE_ITEM(EXYNOS5_CLKDIV_TOP0),
	SAVE_ITEM(EXYNOS5_CLKDIV_TOP1),
	SAVE_ITEM(EXYNOS5_CLKDIV_TOP2),
	SAVE_ITEM(EXYNOS5_CLKDIV_TOP3),
	SAVE_ITEM(EXYNOS5_CLKDIV_FSYS0),
	SAVE_ITEM(EXYNOS5_CLKDIV_FSYS1),
	SAVE_ITEM(EXYNOS5_CLKDIV_FSYS2),
	SAVE_ITEM(EXYNOS5_CLKDIV_GSCL),
	SAVE_ITEM(EXYNOS5_CLKDIV_DISP1_0),
	SAVE_ITEM(EXYNOS5_CLKDIV_DISP1_1),
	SAVE_ITEM(EXYNOS5_CLKDIV_PERIC0),
	SAVE_ITEM(EXYNOS5_CLKDIV_PERIC1),
	SAVE_ITEM(EXYNOS5_CLKDIV_PERIC2),
	SAVE_ITEM(EXYNOS5_CLKDIV_PERIC3),
	SAVE_ITEM(EXYNOS5_CLKDIV_PERIC4),
	SAVE_ITEM(EXYNOS5_CLKDIV_PERIC5),
	SAVE_ITEM(EXYNOS5_CLKDIV_CPERI1),
	SAVE_ITEM(EXYNOS5410_CLKDIV_G2D),
	SAVE_ITEM(EXYNOS5_CLKGATE_BUS_DISP1),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_PERIC),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_PERIS),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_FSYS),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_GSCL0),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_GSCL),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_GEN),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_G2D),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_DISP1),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_MFC),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_G3D),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_CDREX),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_CORE),
	SAVE_ITEM(EXYNOS5_CLKGATE_SCLK_CPU),
	SAVE_ITEM(EXYNOS5_CLKGATE_IP_MSCL),
	SAVE_ITEM(EXYNOS5_CLKSRC_FSYS),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_PERIC0),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_DISP1_0),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_PERIC1),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_MAUDIO),
	SAVE_ITEM(EXYNOS5_CLKGATE_BUS_DISP1),
	SAVE_ITEM(EXYNOS5_CLKGATE_BUS_FSYS0),
	SAVE_ITEM(EXYNOS_PMU_DEBUG),
	SAVE_ITEM(EXYNOS5_CLKSRC_CMUTOP_SPARE2),
};

static struct sleep_save exynos5420_cpll_save[] = {
	SAVE_ITEM(EXYNOS5_CPLL_LOCK),
	SAVE_ITEM(EXYNOS5_CPLL_CON0),
};

static struct sleep_save exynos5420_dpll_save[] = {
	SAVE_ITEM(EXYNOS5_DPLL_LOCK),
	SAVE_ITEM(EXYNOS5_DPLL_CON0),
};

static struct sleep_save exynos5420_ipll_save[] = {
	SAVE_ITEM(EXYNOS5_IPLL_LOCK),
	SAVE_ITEM(EXYNOS5_IPLL_CON0),
	SAVE_ITEM(EXYNOS5_IPLL_CON1),
};

static struct sleep_save exynos5420_epll_save[] = {
	SAVE_ITEM(EXYNOS5_EPLL_LOCK),
	SAVE_ITEM(EXYNOS5_EPLL_CON0),
	SAVE_ITEM(EXYNOS5_EPLL_CON1),
	SAVE_ITEM(EXYNOS5_EPLL_CON2),
};

static struct sleep_save exynos5420_vpll_save[] = {
	SAVE_ITEM(EXYNOS5_VPLL_LOCK),
	SAVE_ITEM(EXYNOS5_VPLL_CON0),
	SAVE_ITEM(EXYNOS5_VPLL_CON1),
};

static struct sleep_save exynos5420_rpll_save[] = {
	SAVE_ITEM(EXYNOS5_RPLL_LOCK),
	SAVE_ITEM(EXYNOS5_RPLL_CON0),
	SAVE_ITEM(EXYNOS5_RPLL_CON1),
};

static struct sleep_save exynos5420_spll_save[] = {
	SAVE_ITEM(EXYNOS5_SPLL_LOCK),
	SAVE_ITEM(EXYNOS5_SPLL_CON0),
	SAVE_ITEM(EXYNOS5_SPLL_CON1),
};

static struct sleep_save exynos5420_mpll_save[] = {
	SAVE_ITEM(EXYNOS5_MPLL_LOCK),
	SAVE_ITEM(EXYNOS5_MPLL_CON0),
};
#endif

/* Disableing makes APLL bypass. enable,1 means bypass. */
static int exynos5_apll_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_APLL_CON1, clk, !enable);
}

/* Disableing makes SPLL bypass. enable,1 means bypass. */
static int exynos5_spll_enable(struct clk *clk, int enable)
{
	if (enable)
		clk->rate = s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS5_SPLL_CON0));
	else
		clk->rate = xtal_rate;

	return s5p_gatectrl(EXYNOS5_SPLL_CON1, clk, !enable);
}

/* Disableing makes VPLL bypass. enable,1 means bypass. */
static int exynos5_vpll_enable(struct clk *clk, int enable)
{
	if (enable)
		clk->rate = s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS5_VPLL_CON0));
	else
		clk->rate = xtal_rate;

	return s5p_gatectrl(EXYNOS5_VPLL_CON1, clk, !enable);
}

/*
 * RPLL on/off control not BYPASS
 * Use only this function to set RPLL off permanently
 */
static int exynos5_rpll_enable(struct clk *clk, int enable)
{
	pr_info("RPLL is %s\n", enable ? "on" : "off");
	return s5p_gatectrl(EXYNOS5_RPLL_CON0, clk, enable);
}

static int exynos5_v_epll_ctrl(struct clk *clk, int enable)
{
	pr_info("EPLL is %s\n", enable ? "on" : "off");
	return s5p_gatectrl(EXYNOS5_EPLL_CON0, clk, enable);
}

static int exynos5_v_vpll_ctrl(struct clk *clk, int enable)
{
	pr_info("VPLL is %s\n", enable ? "on" : "off");
	return s5p_gatectrl(EXYNOS5_VPLL_CON0, clk, enable);
}

static int exynos5_v_ipll_ctrl(struct clk *clk, int enable)
{
	pr_info("IPLL is %s\n", enable ? "on" : "off");
	return s5p_gatectrl(EXYNOS5_IPLL_CON0, clk, enable);
}

static int exynos5_clk_ip_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_FSYS, clk, enable);
}

static int exynos5_clksrc_mask_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_FSYS, clk, enable);
}

static int exynos5_clk_ip_peric_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_PERIC, clk, enable);
}

static int exynos5_clk_ip_peris_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_PERIS, clk, enable);
}

static int exynos5_clksrc_mask_peric0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_PERIC0, clk, enable);
}

static int exynos5_clk_clkout_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS_PMU_DEBUG, clk, !enable);
}

static int exynos5_clksrc_mask_peric1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_PERIC1, clk, enable);
}

static int exynos5_clk_bus_fsys0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_BUS_FSYS0, clk, enable);
}

static int exynos5_clk_bus_cdrex_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_BUS_CDREX, clk, enable);
}

static int exynos5_clk_ip_isp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_ISP0, clk, enable);
}

static int exynos5_clk_ip_isp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_ISP1, clk, enable);
}

static int exynos5_clk_ip_disp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_DISP1, clk, enable);
}

static int exynos5_clk_ip_gscl0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_GSCL0, clk, enable);
}

static int exynos5_clk_ip_gscl1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_GSCL, clk, enable);
}

static int exynos5_clk_ip_gen_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_GEN, clk, enable);
}

static int exynos5_clk_ip_g2d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_G2D, clk, enable);
}

static int exynos5_clk_ip_g3d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_G3D, clk, enable);
}

static int exynos5420_clk_bus_disp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_BUS_DISP1, clk, enable);
}

static int exynos5420_clksrc_mask_disp1_0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_DISP1_0, clk, enable);
}

static int exynos5420_clk_ip_disp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_DISP1, clk, enable);
}

static int exynos5_clk_ip_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_MFC, clk, enable);
}

static int exynos5_clk_ip_mscl_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_MSCL, clk, enable);
}

static int exynos5_epll_ctrl(struct clk *clk, int enable)
{
	if (enable)
		clk->rate = s5p_get_pll36xx(xtal_rate, __raw_readl(EXYNOS5_EPLL_CON0),
					__raw_readl(EXYNOS5_EPLL_CON1));
	else
		clk->rate = xtal_rate;

	return s5p_gatectrl(EXYNOS5_EPLL_CON2, clk, !enable);
}

static int exynos5_clksrc_mask_maudio_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_MAUDIO, clk, enable);
}

static int exynos5_clksrc_mask_isp_sensor_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_TOP_SCLK_ISP, clk, enable);
}

/* Mux output for APLL */
static struct clksrc_clk exynos5420_clk_mout_apll = {
	.clk	= {
		.name		= "mout_apll",
	},
	.sources = &clk_src_apll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CPU, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_sclk_apll = {
	.clk	= {
		.name		= "sclk_apll",
		.parent		= &exynos5420_clk_mout_apll.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 24, .size = 3 },
};

/* Mux output for BPLL */
static struct clksrc_clk exynos5420_clk_mout_bpll = {
	.clk	= {
		.name		= "mout_bpll",
	},
	.sources = &clk_src_bpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CDREX, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_sclk_bpll = {
	.clk	= {
		.name		= "sclk_bpll",
	},
	.sources = &clk_src_bpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CMUTOP_SPARE2, .shift = 0, .size = 1 },
};

/* Mux output for MPLL */
static struct clksrc_clk exynos5420_clk_mout_mpll = {
	.clk	= {
		.name		= "mout_mpll",
	},
	.sources = &clk_src_mpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 0, .size = 1 },
};

/* Mux output for CPLL */
static struct clksrc_clk exynos5420_clk_mout_cpll = {
	.clk	= {
		.name		= "mout_cpll",
	},
	.sources = &clk_src_cpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 28, .size = 1 },
};

/* Mux output for DPLL */
static struct clksrc_clk exynos5420_clk_mout_dpll = {
	.clk	= {
		.name		= "mout_dpll",
	},
	.sources = &clk_src_dpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 24, .size = 1 },
};

/* Mux output for IPLL */
static struct clk *clk_src_ipll_list[] = {
	[0] = &clk_fin_ipll,
	[1] = &exynos5420_clk_fout_ipll.clk,
};

static struct clksrc_sources clk_src_ipll = {
	.sources		= clk_src_ipll_list,
	.nr_sources		= ARRAY_SIZE(clk_src_ipll_list),
};

static struct clksrc_clk exynos5420_clk_mout_ipll = {
	.clk	= {
		.name		= "mout_ipll",
	},
	.sources = &clk_src_ipll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 12, .size = 1 },
};

/* Mux output for VPLL */
static struct clk *clk_src_vpll_list[] = {
	[0] = &clk_fin_vpll,
	[1] = &clk_fout_vpll,
};

static struct clksrc_sources clk_src_vpll = {
	.sources		= clk_src_vpll_list,
	.nr_sources		= ARRAY_SIZE(clk_src_vpll_list),
};

static struct clksrc_clk exynos5420_clk_mout_vpll = {
	.clk	= {
		.name		= "mout_vpll",
	},
	.sources = &clk_src_vpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 4, .size = 1 },
};

/* Mux output for EPLL */
static struct clksrc_clk exynos5420_clk_mout_epll = {
	.clk	= {
		.name		= "mout_epll",
	},
	.sources = &clk_src_epll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 20, .size = 1 },
};

/* Mux output for RPLL */
static struct clk *clk_src_rpll_list[] = {
	[0] = &clk_fin_rpll,
	[1] = &clk_fout_rpll,
};

static struct clksrc_sources clk_src_rpll = {
	.sources		= clk_src_rpll_list,
	.nr_sources		= ARRAY_SIZE(clk_src_rpll_list),
};

static struct clksrc_clk exynos5420_clk_mout_rpll = {
	.clk	= {
		.name		= "mout_rpll",
	},
	.sources = &clk_src_rpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 16, .size = 1 },
};

/* Mux output for SPLL */
static struct clk *clk_src_spll_list[] = {
	[0] = &clk_fin_spll,
	[1] = &clk_fout_spll,
};

static struct clksrc_sources clk_src_spll = {
	.sources		= clk_src_spll_list,
	.nr_sources		= ARRAY_SIZE(clk_src_spll_list),
};

static struct clksrc_clk exynos5420_clk_mout_spll = {
	.clk	= {
		.name		= "mout_spll",
	},
	.sources = &clk_src_spll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP6, .shift = 8, .size = 1 },
};

/* MX_MSPLL clock list*/
static struct clk *exynos5420_clkset_mx_mspll_list[] = {
	[0] = &exynos5420_clk_mout_cpll.clk,
	[1] = &exynos5420_clk_mout_dpll.clk,
	[2] = &exynos5420_clk_mout_mpll.clk,
	[3] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_mx_mspll = {
	.sources	= exynos5420_clkset_mx_mspll_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mx_mspll_list),
};

/* MX_MSPLL_CPU */
static struct clksrc_clk exynos5420_mx_mspll_cpu = {
	.clk	= {
		.name		= "mx_mspll_cpu",
	},
	.sources = &exynos5420_clkset_mx_mspll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP7, .shift = 12, .size = 2 },
};

static struct clk *exynos5420_clkset_mout_cpu_list[] = {
	[0] = &exynos5420_clk_mout_apll.clk,
	[1] = &exynos5420_mx_mspll_cpu.clk,
};

static struct clksrc_sources exynos5420_clkset_mout_cpu = {
	.sources	= exynos5420_clkset_mout_cpu_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mout_cpu_list),
};

/* CPU Block */
static struct clksrc_clk exynos5420_clk_mout_cpu = {
	.clk	= {
		.name		= "mout_cpu",
	},
	.sources = &exynos5420_clkset_mout_cpu,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CPU, .shift = 16, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_armclk = {
	.clk	= {
		.name		= "armclk",
		.parent		= &exynos5420_clk_mout_cpu.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 28, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_aclk_cpud = {
	.clk	= {
		.name		= "aclk_cpud",
		.parent		= &exynos5420_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_pclk_dbg = {
	.clk	= {
		.name		= "pclk_dbg",
		.parent		= &exynos5420_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 20, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_atclk = {
	.clk	= {
		.name		= "atclk",
		.parent		= &exynos5420_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 16, .size = 3 },
};

static struct clksrc_sources exynos5420_clkset_mout_hpm_cpu = {
	.sources		= exynos5420_clkset_mout_cpu_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mout_cpu_list),
};

static struct clksrc_clk exynos5420_clk_mout_hpm_cpu = {
	.clk	= {
		.name		= "mout_hpm_cpu",
	},
	.sources = &exynos5420_clkset_mout_hpm_cpu,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CPU, .shift = 20, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_dout_copy = {
	.clk	= {
		.name		= "dout_copy",
		.parent		= &exynos5420_clk_mout_hpm_cpu.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU1, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_sclk_hpm = {
	.clk	= {
		.name		= "sclk_hpm",
		.parent		= &exynos5420_clk_dout_copy.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU1, .shift = 4, .size = 3 },
};

/* KFC Block */
/* Mux output for KPLL */
static struct clk *clk_src_kpll_list[] = {
	[0] = &clk_fin_kpll,
	[1] = &clk_fout_kpll,
};

static struct clksrc_sources clk_src_kpll = {
	.sources		= clk_src_kpll_list,
	.nr_sources	= ARRAY_SIZE(clk_src_kpll_list),
};

static struct clksrc_clk exynos5420_clk_mout_kpll = {
	.clk	= {
		.name		= "mout_kpll",
	},
	.sources = &clk_src_kpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_KFC, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_sclk_kpll = {
	.clk	= {
		.name		= "sclk_kpll",
		.parent		= &exynos5420_clk_mout_kpll.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_KFC0, .shift = 24, .size = 3 },
};

/* MX_MSPLL_KFC */
static struct clksrc_clk exynos5420_mx_mspll_kfc = {
	.clk	= {
		.name		= "mx_mspll_kfc",
	},
	.sources = &exynos5420_clkset_mx_mspll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP7, .shift = 8, .size = 2 },
};

static struct clk *exynos5420_clkset_mout_kfc_list[] = {
	[0] = &exynos5420_clk_mout_kpll.clk,
	[1] = &exynos5420_mx_mspll_kfc.clk,
};

static struct clksrc_sources exynos5420_clkset_mout_kfc = {
	.sources		= exynos5420_clkset_mout_kfc_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mout_kfc_list),
};

static struct clksrc_clk exynos5420_clk_mout_kfc = {
	.clk	= {
		.name		= "mout_kfc",
	},
	.sources = &exynos5420_clkset_mout_kfc,
	.reg_src = { .reg = EXYNOS5_CLKSRC_KFC, .shift = 16, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_kfc_armclk = {
	.clk	= {
		.name		= "kfc_armclk",
		.parent		= &exynos5420_clk_mout_kfc.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_KFC0, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_aclk_kfc = {
	.clk	= {
		.name		= "aclk_kfc",
		.parent		= &exynos5420_clk_kfc_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_KFC0, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_pclk_kfc = {
	.clk	= {
		.name		= "pclk_kfc",
		.parent		= &exynos5420_clk_kfc_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_KFC0, .shift = 20, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_mout_hpm_kfc = {
	.clk	= {
		.name		= "mout_hpm_kfc",
	},
	.sources = &exynos5420_clkset_mout_kfc,
	.reg_src = { .reg = EXYNOS5_CLKSRC_KFC, .shift = 15, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_sclk_hpm_kfc = {
	.clk	= {
		.name		= "sclk_hpm_kfc",
		.parent		= &exynos5420_clk_mout_hpm_kfc.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_KFC0, .shift = 8, .size = 3 },
};

/* MX_MSPLL_CCORE */
static struct clksrc_clk exynos5420_mx_mspll_ccore = {
	.clk	= {
		.name		= "mx_mspll_ccore",
	},
	.sources = &exynos5420_clkset_mx_mspll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP7, .shift = 16, .size = 2 },
};

/* CDREX BLOCK */
static struct clk *exynos5420_clkset_mclk_cdrex_list[] = {
	[0] = &exynos5420_clk_mout_bpll.clk,
	[1] = &exynos5420_mx_mspll_ccore.clk,
};

static struct clksrc_sources exynos5420_clkset_mclk_cdrex = {
	.sources		= exynos5420_clkset_mclk_cdrex_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mclk_cdrex_list),
};

static struct clksrc_clk exynos5420_clk_mclk_cdrex = {
	.clk	= {
		.name		= "mclk_cdrex",
	},
	.sources = &exynos5420_clkset_mclk_cdrex,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CDREX, .shift = 4, .size = 1 },
};

static struct clksrc_clk exynos5420_clk_sclk_cdrex = {
	.clk	= {
		.name		= "sclk_cdrex",
		.parent		= &exynos5420_clk_mclk_cdrex.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 24, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_clk2x_phy0 = {
	.clk	= {
		.name		= "clk2x_phy0",
		.parent		= &exynos5420_clk_sclk_cdrex.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 3, .size = 5 },
};

static struct clksrc_clk exynos5420_clk_cclk_drex0 = {
	.clk	= {
		.name		= "cclk_drex0",
		.parent		= &exynos5420_clk_clk2x_phy0.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_cclk_drex1 = {
	.clk	= {
		.name		= "cclk_drex1",
		.parent		= &exynos5420_clk_clk2x_phy0.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_aclk_cdrex1 = {
	.clk	= {
		.name		= "aclk_cdrex1",
		.parent		= &exynos5420_clk_clk2x_phy0.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 16, .size = 3 },
};

/* TOP BLOCK */

/* Clocks for xCLK_cdm */
static struct clk *exynos5420_clkset_xclk_cdm_list[] = {
	[0] = &exynos5420_clk_mout_cpll.clk,
	[1] = &exynos5420_clk_mout_dpll.clk,
	[2] = &exynos5420_clk_mout_mpll.clk,
};

static struct clksrc_sources exynos5420_clkset_xclk_cdm = {
	.sources	= exynos5420_clkset_xclk_cdm_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_xclk_cdm_list),
};

/* Clocks for ACLK_idm */
static struct clk *exynos5420_clkset_aclk_idm_list[] = {
	[0] = &exynos5420_clk_mout_ipll.clk,
	[1] = &exynos5420_clk_mout_dpll.clk,
	[2] = &exynos5420_clk_mout_mpll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_idm = {
	.sources	= exynos5420_clkset_aclk_idm_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_idm_list),
};

/* ACLK_200_FSYS */
static struct clksrc_clk exynos5420_aclk_200_fsys_dout = {
	.clk	= {
		.name		= "aclk_200_fsys_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 28, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 28, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_200_fsys_sw_list[] = {
	[0] = &exynos5420_aclk_200_fsys_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_200_fsys_sw = {
	.sources	= exynos5420_clkset_aclk_200_fsys_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_200_fsys_sw_list),
};

static struct clksrc_clk exynos5420_aclk_200_fsys_sw = {
	.clk	= {
		.name		= "aclk_200_fsys_sw",
	},
	.sources = &exynos5420_clkset_aclk_200_fsys_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 28, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_200_fsys_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_200_fsys_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_200_fsys_user = {
	.sources	= exynos5420_clkset_aclk_200_fsys_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_200_fsys_user_list),
};

static struct clksrc_clk exynos5420_aclk_200_fsys = {
	.clk	= {
		.name		= "aclk_200_fsys",
	},
	.sources = &exynos5420_clkset_aclk_200_fsys_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 28, .size = 1 },
};

/* PCLK_200_RSTOP_FSYS */
static struct clksrc_clk exynos5420_pclk_200_fsys_dout = {
	.clk	= {
		.name		= "pclk_200_fsys_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 24, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 24, .size = 3 },
};

static struct clk *exynos5420_clkset_pclk_200_fsys_sw_list[] = {
	[0] = &exynos5420_pclk_200_fsys_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_pclk_200_fsys_sw = {
	.sources	= exynos5420_clkset_pclk_200_fsys_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_pclk_200_fsys_sw_list),
};

static struct clksrc_clk exynos5420_pclk_200_fsys_sw = {
	.clk	= {
		.name		= "pclk_200_fsys_sw",
	},
	.sources = &exynos5420_clkset_pclk_200_fsys_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 24, .size = 1 },
};

static struct clk *exynos5420_clkset_pclk_200_fsys_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_pclk_200_fsys_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_pclk_200_fsys_user = {
	.sources	= exynos5420_clkset_pclk_200_fsys_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_pclk_200_fsys_user_list),
};

static struct clksrc_clk exynos5420_pclk_200_fsys = {
	.clk	= {
		.name		= "pclk_200_fsys",
	},
	.sources = &exynos5420_clkset_pclk_200_fsys_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 24, .size = 1 },
};

/* ACLK_100_NOC */
static struct clksrc_clk exynos5420_aclk_100_noc_dout = {
	.clk	= {
		.name		= "aclk_100_noc_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 20, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 20, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_100_noc_sw_list[] = {
	[0] = &exynos5420_aclk_100_noc_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_100_noc_sw = {
	.sources	= exynos5420_clkset_aclk_100_noc_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_100_noc_sw_list),
};

static struct clksrc_clk exynos5420_aclk_100_noc_sw = {
	.clk	= {
		.name		= "aclk_100_noc_sw",
	},
	.sources = &exynos5420_clkset_aclk_100_noc_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 20, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_100_noc_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_100_noc_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_100_noc_user = {
	.sources	= exynos5420_clkset_aclk_100_noc_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_100_noc_user_list),
};

static struct clksrc_clk exynos5420_aclk_100_noc = {
	.clk	= {
		.name		= "aclk_100_noc",
	},
	.sources = &exynos5420_clkset_aclk_100_noc_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 20, .size = 1 },
};

/* ACLK_400_WCORE */
static struct clksrc_clk exynos5420_aclk_400_wcore_mout = {
	.clk	= {
		.name		= "aclk_400_wcore_mout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 16, .size = 2 },
};

static struct clk *exynos5420_clkset_aclk_400_wcore_bpll_list[] = {
	[0] = &exynos5420_aclk_400_wcore_mout.clk,
	[1] = &exynos5420_clk_sclk_bpll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_wcore_bpll = {
	.sources	= exynos5420_clkset_aclk_400_wcore_bpll_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_wcore_bpll_list),
};

static struct clksrc_clk exynos5420_aclk_400_wcore_dout = {
	.clk    = {
	.name           = "aclk_400_wcore_dout",
	},
	.sources = &exynos5420_clkset_aclk_400_wcore_bpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CMUTOP_SPARE2, .shift = 4, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 16, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_400_wcore_sw_list[] = {
	[0] = &exynos5420_aclk_400_wcore_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_wcore_sw = {
	.sources	= exynos5420_clkset_aclk_400_wcore_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_wcore_sw_list),
};

static struct clksrc_clk exynos5420_aclk_400_wcore_sw = {
	.clk	= {
		.name		= "aclk_400_wcore_sw",
	},
	.sources = &exynos5420_clkset_aclk_400_wcore_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 16, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_400_wcore_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_400_wcore_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_wcore_user = {
	.sources	= exynos5420_clkset_aclk_400_wcore_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_wcore_user_list),
};

static struct clksrc_clk exynos5420_aclk_400_wcore = {
	.clk	= {
		.name		= "aclk_400_wcore",
	},
	.sources = &exynos5420_clkset_aclk_400_wcore_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 16, .size = 1 },
};

/* ACLK_200_FSYS2 */
static struct clksrc_clk exynos5420_aclk_200_fsys2_dout = {
	.clk	= {
		.name		= "aclk_200_fsys2_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 12, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 12, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_200_fsys2_sw_list[] = {
	[0] = &exynos5420_aclk_200_fsys2_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_200_fsys2_sw = {
	.sources	= exynos5420_clkset_aclk_200_fsys2_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_200_fsys2_sw_list),
};

static struct clksrc_clk exynos5420_aclk_200_fsys2_sw = {
	.clk	= {
		.name		= "aclk_200_fsys2_sw",
	},
	.sources = &exynos5420_clkset_aclk_200_fsys2_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 12, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_200_fsys2_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_200_fsys2_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_200_fsys2_user = {
	.sources	= exynos5420_clkset_aclk_200_fsys2_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_200_fsys2_user_list),
};

static struct clksrc_clk exynos5420_aclk_200_fsys2 = {
	.clk	= {
		.name		= "aclk_200_fsys2",
	},
	.sources = &exynos5420_clkset_aclk_200_fsys2_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 12, .size = 1 },
};

/* ACLK_200_DISP1 */
static struct clksrc_clk exynos5420_aclk_200_dout = {
	.clk	= {
		.name		= "aclk_200_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 8, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 8, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_200_sw_list[] = {
	[0] = &exynos5420_aclk_200_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_200_sw = {
	.sources	= exynos5420_clkset_aclk_200_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_200_sw_list),
};

static struct clksrc_clk exynos5420_aclk_200_sw = {
	.clk	= {
		.name		= "aclk_200_sw",
	},
	.sources = &exynos5420_clkset_aclk_200_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 8, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_200_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_200_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_200_user = {
	.sources	= exynos5420_clkset_aclk_200_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_200_user_list),
};

static struct clksrc_clk exynos5420_aclk_200_disp1 = {
	.clk	= {
		.name		= "aclk_200_disp1",
	},
	.sources = &exynos5420_clkset_aclk_200_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 8, .size = 1 },
};

/* ACLK_400_MSCL */
static struct clksrc_clk exynos5420_aclk_400_mscl_dout = {
	.clk	= {
		.name		= "aclk_400_mscl_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 4, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 4, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_400_mscl_sw_list[] = {
	[0] = &exynos5420_aclk_400_mscl_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_mscl_sw = {
	.sources	= exynos5420_clkset_aclk_400_mscl_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_mscl_sw_list),
};

static struct clksrc_clk exynos5420_aclk_400_mscl_sw = {
	.clk	= {
		.name		= "aclk_400_mscl_sw",
	},
	.sources = &exynos5420_clkset_aclk_400_mscl_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 4, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_400_mscl_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_400_mscl_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_mscl_user = {
	.sources	= exynos5420_clkset_aclk_400_mscl_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_mscl_user_list),
};

static struct clksrc_clk exynos5420_aclk_400_mscl = {
	.clk	= {
		.name		= "aclk_400_mscl",
	},
	.sources = &exynos5420_clkset_aclk_400_mscl_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 4, .size = 1 },
};

/* ACLK_400_ISP */
static struct clksrc_clk exynos5420_aclk_400_isp_dout = {
	.clk	= {
		.name		= "aclk_400_isp_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 0, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 0, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_400_isp_sw_list[] = {
	[0] = &exynos5420_aclk_400_isp_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_isp_sw = {
	.sources	= exynos5420_clkset_aclk_400_isp_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_isp_sw_list),
};

static struct clksrc_clk exynos5420_aclk_400_isp_sw = {
	.clk	= {
		.name		= "aclk_400_isp_sw",
	},
	.sources = &exynos5420_clkset_aclk_400_isp_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP10, .shift = 0, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_400_isp_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_400_isp_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_isp_user = {
	.sources	= exynos5420_clkset_aclk_400_isp_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_isp_user_list),
};

static struct clksrc_clk exynos5420_aclk_400_isp = {
	.clk	= {
		.name		= "aclk_400_isp",
	},
	.sources = &exynos5420_clkset_aclk_400_isp_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 0, .size = 1 },
};

/* ACLK_333 */
static struct clksrc_clk exynos5420_aclk_333_dout = {
	.clk	= {
		.name		= "aclk_333_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 28, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 28, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_333_sw_list[] = {
	[0] = &exynos5420_aclk_333_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_sw = {
	.sources	= exynos5420_clkset_aclk_333_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_sw_list),
};

static struct clksrc_clk exynos5420_aclk_333_sw = {
	.clk	= {
		.name		= "aclk_333_sw",
	},
	.sources = &exynos5420_clkset_aclk_333_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 28, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_333_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_333_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_user = {
	.sources	= exynos5420_clkset_aclk_333_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_user_list),
};

static struct clksrc_clk exynos5420_aclk_333 = {
	.clk	= {
		.name		= "aclk_333",
	},
	.sources = &exynos5420_clkset_aclk_333_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 28, .size = 1 },
};

/* ACLK_333 */
static struct clksrc_clk exynos5420_aclk_166_dout = {
	.clk	= {
		.name		= "aclk_166_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 24, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 24, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_166_sw_list[] = {
	[0] = &exynos5420_aclk_166_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_166_sw = {
	.sources	= exynos5420_clkset_aclk_166_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_166_sw_list),
};

static struct clksrc_clk exynos5420_aclk_166_sw = {
	.clk	= {
		.name		= "aclk_166_sw",
	},
	.sources = &exynos5420_clkset_aclk_166_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 24, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_166_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_166_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_166_user = {
	.sources	= exynos5420_clkset_aclk_166_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_166_user_list),
};

static struct clksrc_clk exynos5420_aclk_166 = {
	.clk	= {
		.name		= "aclk_166",
	},
	.sources = &exynos5420_clkset_aclk_166_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 24, .size = 1 },
};

/* ACLK_266 */
static struct clksrc_clk exynos5420_aclk_266_dout = {
	.clk	= {
		.name		= "aclk_266_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 20, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 20, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_266_sw_list[] = {
	[0] = &exynos5420_aclk_266_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_266_sw = {
	.sources	= exynos5420_clkset_aclk_266_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_266_sw_list),
};

static struct clksrc_clk exynos5420_aclk_266_sw = {
	.clk	= {
		.name		= "aclk_266_sw",
	},
	.sources = &exynos5420_clkset_aclk_266_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 20, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_266_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_266_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_266_user = {
	.sources	= exynos5420_clkset_aclk_266_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_266_user_list),
};

static struct clksrc_clk exynos5420_aclk_266 = {
	.clk	= {
		.name		= "aclk_266",
	},
	.sources = &exynos5420_clkset_aclk_266_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 20, .size = 1 },
};

/* ACLK_266_ISP */
static struct clksrc_clk exynos5420_aclk_266_isp = {
	.clk	= {
		.name		= "aclk_266_isp",
	},
	.sources = &exynos5420_clkset_aclk_266_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 16, .size = 1 },
};

/* ACLK_66 */
static struct clksrc_clk exynos5420_aclk_66_dout = {
	.clk	= {
		.name		= "aclk_66_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 8, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 8, .size = 6 },
};

static struct clk *exynos5420_clkset_aclk_66_sw_list[] = {
	[0] = &exynos5420_aclk_66_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_66_sw = {
	.sources	= exynos5420_clkset_aclk_66_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_66_sw_list),
};

static struct clksrc_clk exynos5420_aclk_66_sw = {
	.clk	= {
		.name		= "aclk_66_sw",
	},
	.sources = &exynos5420_clkset_aclk_66_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 8, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_66_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_66_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_66_user = {
	.sources	= exynos5420_clkset_aclk_66_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_66_user_list),
};

static struct clksrc_clk exynos5420_aclk_66_peric = {
	.clk	= {
		.name		= "aclk_66_peric",
	},
	.sources = &exynos5420_clkset_aclk_66_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 8, .size = 1 },
};


static struct clksrc_clk exynos5420_aclk_66_psgen = {
	.clk	= {
		.name		= "aclk_66_psgen",
	},
	.sources = &exynos5420_clkset_aclk_66_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 4, .size = 1 },
};

static unsigned long exynos5420_aclk_66_ff2_get_rate(struct clk *clk)
{
	return clk_get_rate(&exynos5420_aclk_66_sw.clk) / 2;
};

static struct clk_ops exynos5420_aclk_66_ff2_ops = {
	.get_rate = exynos5420_aclk_66_ff2_get_rate,
};

static struct clk exynos5420_aclk_66_ff2 = {
	.name	= "aclk_66_ff2",
	.id	= -1,
	.ops	= &exynos5420_aclk_66_ff2_ops,
};

static struct clk *exynos5420_clkset_aclk_66_gpio_user_list[] = {
	[0] = &exynos5420_aclk_66_sw.clk,
	[1] = &exynos5420_aclk_66_ff2,
};

static struct clksrc_sources exynos5420_clkset_aclk_66_gpio_user = {
	.sources	= exynos5420_clkset_aclk_66_gpio_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_66_gpio_user_list),
};

static struct clksrc_clk exynos5420_aclk_66_gpio = {
	.clk	= {
		.name		= "aclk_66_gpio",
	},
	.sources = &exynos5420_clkset_aclk_66_gpio_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP7, .shift = 4, .size = 1 },
};

/* ACLK_333_432_ISP0 */
static struct clksrc_clk exynos5420_aclk_333_432_isp0_dout = {
	.clk	= {
		.name		= "aclk_333_432_isp0_dout",
	},
	.sources = &exynos5420_clkset_aclk_idm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 12, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 16, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_333_432_isp0_sw_list[] = {
	[0] = &exynos5420_aclk_333_432_isp0_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_432_isp0_sw = {
	.sources	= exynos5420_clkset_aclk_333_432_isp0_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_432_isp0_sw_list),
};

static struct clksrc_clk exynos5420_aclk_333_432_isp0_sw = {
	.clk	= {
		.name		= "aclk_333_432_isp0_sw",
	},
	.sources = &exynos5420_clkset_aclk_333_432_isp0_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 12, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_333_432_isp0_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_333_432_isp0_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_432_isp0_user = {
	.sources	= exynos5420_clkset_aclk_333_432_isp0_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_432_isp0_user_list),
};

static struct clksrc_clk exynos5420_aclk_333_432_isp0 = {
	.clk	= {
		.name		= "aclk_333_432_isp0",
	},
	.sources = &exynos5420_clkset_aclk_333_432_isp0_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 12, .size = 1 },
};

/* ACLK_333_432_ISP */
static struct clksrc_clk exynos5420_aclk_333_432_isp_dout = {
	.clk	= {
		.name		= "aclk_333_432_isp_dout",
	},
	.sources = &exynos5420_clkset_aclk_idm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 4, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 4, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_333_432_isp_sw_list[] = {
	[0] = &exynos5420_aclk_333_432_isp_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_432_isp_sw = {
	.sources	= exynos5420_clkset_aclk_333_432_isp_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_432_isp_sw_list),
};

static struct clksrc_clk exynos5420_aclk_333_432_isp_sw = {
	.clk	= {
		.name		= "aclk_333_432_isp_sw",
	},
	.sources = &exynos5420_clkset_aclk_333_432_isp_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 4, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_333_432_isp_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_333_432_isp_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_432_isp_user = {
	.sources	= exynos5420_clkset_aclk_333_432_isp_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_432_isp_user_list),
};

static struct clksrc_clk exynos5420_aclk_333_432_isp = {
	.clk	= {
		.name		= "aclk_333_432_isp",
	},
	.sources = &exynos5420_clkset_aclk_333_432_isp_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 4, .size = 1 },
};

/* ACLK_333_432_GSCL */
static struct clksrc_clk exynos5420_aclk_333_432_gscl_dout = {
	.clk	= {
		.name		= "aclk_333_432_gscl_dout",
	},
	.sources = &exynos5420_clkset_aclk_idm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP1, .shift = 0, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 0, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_333_432_gscl_sw_list[] = {
	[0] = &exynos5420_aclk_333_432_gscl_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_432_gscl_sw = {
	.sources	= exynos5420_clkset_aclk_333_432_gscl_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_432_gscl_sw_list),
};

static struct clksrc_clk exynos5420_aclk_333_432_gscl_sw = {
	.clk	= {
		.name		= "aclk_333_432_gscl_sw",
	},
	.sources = &exynos5420_clkset_aclk_333_432_gscl_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP11, .shift = 0, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_333_432_gscl_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_333_432_gscl_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_432_gscl_user = {
	.sources	= exynos5420_clkset_aclk_333_432_gscl_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_432_gscl_user_list),
};

static struct clksrc_clk exynos5420_aclk_333_432_gscl = {
	.clk	= {
		.name		= "aclk_333_432_gscl",
	},
	.sources = &exynos5420_clkset_aclk_333_432_gscl_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP4, .shift = 0, .size = 1 },
};

/* ACLK_300_GSCL */
static struct clksrc_clk exynos5420_aclk_300_gscl_dout = {
	.clk	= {
		.name		= "aclk_300_gscl_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 28, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 28, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_300_gscl_sw_list[] = {
	[0] = &exynos5420_aclk_300_gscl_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_300_gscl_sw = {
	.sources	= exynos5420_clkset_aclk_300_gscl_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_300_gscl_sw_list),
};

static struct clksrc_clk exynos5420_aclk_300_gscl_sw = {
	.clk	= {
		.name		= "aclk_300_gscl_sw",
	},
	.sources = &exynos5420_clkset_aclk_300_gscl_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 28, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_300_gscl_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_300_gscl_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_300_gscl_user = {
	.sources	= exynos5420_clkset_aclk_300_gscl_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_300_gscl_user_list),
};

static struct clksrc_clk exynos5420_aclk_300_gscl = {
	.clk	= {
		.name		= "aclk_300_gscl",
	},
	.sources = &exynos5420_clkset_aclk_300_gscl_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 28, .size = 1 },
};

/* ACLK_300_DISP1 */
static struct clksrc_clk exynos5420_aclk_300_disp1_dout = {
	.clk	= {
		.name		= "aclk_300_disp1_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 24, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 24, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_300_disp1_sw_list[] = {
	[0] = &exynos5420_aclk_300_disp1_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_300_disp1_sw = {
	.sources	= exynos5420_clkset_aclk_300_disp1_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_300_disp1_sw_list),
};

static struct clksrc_clk exynos5420_aclk_300_disp1_sw = {
	.clk	= {
		.name		= "aclk_300_disp1_sw",
	},
	.sources = &exynos5420_clkset_aclk_300_disp1_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 24, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_300_disp1_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_300_disp1_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_300_disp1_user = {
	.sources	= exynos5420_clkset_aclk_300_disp1_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_300_disp1_user_list),
};

static struct clksrc_clk exynos5420_aclk_300_disp1 = {
	.clk	= {
		.name		= "aclk_300_disp1",
	},
	.sources = &exynos5420_clkset_aclk_300_disp1_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 24, .size = 1 },
};

/* ACLK_400_DISP1 */
static struct clksrc_clk exynos5420_aclk_400_disp1_dout = {
	.clk	= {
		.name		= "aclk_400_disp1_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 4, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 4, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_400_disp1_sw_list[] = {
	[0] = &exynos5420_aclk_400_disp1_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_disp1_sw = {
	.sources	= exynos5420_clkset_aclk_400_disp1_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_disp1_sw_list),
};

static struct clksrc_clk exynos5420_aclk_400_disp1_sw = {
	.clk	= {
		.name		= "aclk_400_disp1_sw",
	},
	.sources = &exynos5420_clkset_aclk_400_disp1_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 4, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_400_disp1_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_400_disp1_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_400_disp1_user = {
	.sources	= exynos5420_clkset_aclk_400_disp1_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_400_disp1_user_list),
};

static struct clksrc_clk exynos5420_aclk_400_disp1 = {
	.clk	= {
		.name		= "aclk_400_disp1",
	},
	.sources = &exynos5420_clkset_aclk_400_disp1_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 0, .size = 1 },
};

/* ACLK_300_JPEG */
static struct clksrc_clk exynos5420_aclk_300_jpeg_dout = {
	.clk	= {
		.name		= "aclk_300_jpeg_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 20, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 20, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_300_jpeg_sw_list[] = {
	[0] = &exynos5420_aclk_300_jpeg_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_300_jpeg_sw = {
	.sources	= exynos5420_clkset_aclk_300_jpeg_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_300_jpeg_sw_list),
};

static struct clksrc_clk exynos5420_aclk_300_jpeg_sw = {
	.clk	= {
		.name		= "aclk_300_jpeg_sw",
	},
	.sources = &exynos5420_clkset_aclk_300_jpeg_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 20, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_300_jpeg_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_300_jpeg_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_300_jpeg_user = {
	.sources	= exynos5420_clkset_aclk_300_jpeg_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_300_jpeg_user_list),
};

static struct clksrc_clk exynos5420_aclk_300_jpeg = {
	.clk	= {
		.name		= "aclk_300_jpeg",
	},
	.sources = &exynos5420_clkset_aclk_300_jpeg_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 20, .size = 1 },
};

/* ACLK_G3D */
static struct clk *exynos5420_clkset_aclk_g3d_pre_list[] = {
	[0] = &exynos5420_clk_mout_vpll.clk,
	[1] = &exynos5420_clk_mout_dpll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_g3d_pre = {
	.sources	= exynos5420_clkset_aclk_g3d_pre_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_g3d_pre_list),
};

static struct clksrc_clk exynos5420_aclk_g3d_dout = {
	.clk	= {
		.name		= "aclk_g3d_dout",
	},
	.sources = &exynos5420_clkset_aclk_g3d_pre,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 16, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 16, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_g3d_sw_list[] = {
	[0] = &exynos5420_aclk_g3d_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_g3d_sw = {
	.sources	= exynos5420_clkset_aclk_g3d_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_g3d_sw_list),
};

static struct clksrc_clk exynos5420_aclk_g3d_sw = {
	.clk	= {
		.name		= "aclk_g3d_sw",
	},
	.sources = &exynos5420_clkset_aclk_g3d_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 16, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_g3d_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_g3d_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_g3d_user = {
	.sources	= exynos5420_clkset_aclk_g3d_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_g3d_user_list),
};

static struct clksrc_clk exynos5420_aclk_g3d = {
	.clk	= {
		.name		= "aclk_g3d",
	},
	.sources = &exynos5420_clkset_aclk_g3d_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 16, .size = 1 },
};

/* ACLK_266_G2D */
static struct clksrc_clk exynos5420_aclk_266_g2d_dout = {
	.clk	= {
		.name		= "aclk_266_g2d_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 12, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 12, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_266_g2d_sw_list[] = {
	[0] = &exynos5420_aclk_266_g2d_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_266_g2d_sw = {
	.sources	= exynos5420_clkset_aclk_266_g2d_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_266_g2d_sw_list),
};

static struct clksrc_clk exynos5420_aclk_266_g2d_sw = {
	.clk	= {
		.name		= "aclk_266_g2d_sw",
	},
	.sources = &exynos5420_clkset_aclk_266_g2d_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 12, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_266_g2d_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_266_g2d_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_266_g2d_user = {
	.sources	= exynos5420_clkset_aclk_266_g2d_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_266_g2d_user_list),
};

static struct clksrc_clk exynos5420_aclk_266_g2d = {
	.clk	= {
		.name		= "aclk_266_g2d",
	},
	.sources = &exynos5420_clkset_aclk_266_g2d_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 12, .size = 1 },
};

/* ACLK_333_G2D */
static struct clksrc_clk exynos5420_aclk_333_g2d_dout = {
	.clk	= {
		.name		= "aclk_333_g2d_dout",
	},
	.sources = &exynos5420_clkset_xclk_cdm,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 8, .size = 2 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP2, .shift = 8, .size = 3 },
};

static struct clk *exynos5420_clkset_aclk_333_g2d_sw_list[] = {
	[0] = &exynos5420_aclk_333_g2d_dout.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_g2d_sw = {
	.sources	= exynos5420_clkset_aclk_333_g2d_sw_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_g2d_sw_list),
};

static struct clksrc_clk exynos5420_aclk_333_g2d_sw = {
	.clk	= {
		.name		= "aclk_333_g2d_sw",
	},
	.sources = &exynos5420_clkset_aclk_333_g2d_sw,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP12, .shift = 8, .size = 1 },
};

static struct clk *exynos5420_clkset_aclk_333_g2d_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5420_aclk_333_g2d_sw.clk,
};

static struct clksrc_sources exynos5420_clkset_aclk_333_g2d_user = {
	.sources	= exynos5420_clkset_aclk_333_g2d_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_aclk_333_g2d_user_list),
};

static struct clksrc_clk exynos5420_aclk_333_g2d = {
	.clk	= {
		.name		= "aclk_333_g2d",
	},
	.sources = &exynos5420_clkset_aclk_333_g2d_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP5, .shift = 8, .size = 1 },
};

/* MAU_EPLL_CLK */
static struct clk *exynos5420_clkset_mau_epll_clk_list[] = {
	[0] = &exynos5420_clk_mout_epll.clk,
	[1] = &exynos5420_clk_mout_dpll.clk,
	[2] = &exynos5420_clk_mout_mpll.clk,
	[3] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_mau_epll_clk = {
	.sources	= exynos5420_clkset_mau_epll_clk_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mau_epll_clk_list),
};

static struct clksrc_clk exynos5420_mau_epll_clk = {
	.clk	= {
		.name		= "mau_epll_clk",
	},
	.sources = &exynos5420_clkset_mau_epll_clk,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP7, .shift = 20, .size = 2 },
};

static struct clk *exynos5420_clkset_group_list[] = {
	[0] = &clk_xtal,
	[1] = &exynos5420_clk_mout_cpll.clk,
	[2] = &exynos5420_clk_mout_dpll.clk,
	[3] = &exynos5420_clk_mout_mpll.clk,
	[4] = &exynos5420_clk_mout_spll.clk,
	[5] = &exynos5420_clk_mout_ipll.clk,
	[6] = &exynos5420_clk_mout_epll.clk,
	[7] = &exynos5420_clk_mout_rpll.clk,
};

static struct clksrc_sources exynos5420_clkset_group = {
	.sources	= exynos5420_clkset_group_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_group_list),
};

static struct clk exynos5_clk_pdma0 = {
	.name		= "dma",
	.devname	= "dma-pl330.0",
	.enable		= exynos5_clk_bus_fsys0_ctrl,
	.ctrlbit	= (1 << 1),
};

static struct clk exynos5_clk_pdma1 = {
	.name		= "dma",
	.devname	= "dma-pl330.1",
	.enable		= exynos5_clk_bus_fsys0_ctrl,
	.ctrlbit	= (1 << 2),
};

static struct clk exynos5_clk_mdma = {
	.name		= "dma",
	.devname	= "dma-pl330.2",
	.enable		= exynos5_clk_ip_g2d_ctrl,
	.ctrlbit	= (1 << 1) | (1 << 8),
};

static struct clk exynos5_clk_adma0 = {
	.name		= "dma",
	.devname	= "dma-pl330.3",
};

static struct clk *exynos5420_clkset_sclk_fimd1_list[] = {
	[0] = &exynos5420_clk_mout_rpll.clk,
	[1] = &exynos5420_clk_mout_spll.clk,
};

static struct clksrc_sources exynos5420_clkset_sclk_fimd1 = {
	.sources	= exynos5420_clkset_sclk_fimd1_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_sclk_fimd1_list),
};

static struct clksrc_clk exynos5420_clk_mout_mdnie1 = {
	.clk		= {
		.name		= "mout_mdnie1",
	},
	.sources = &exynos5420_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_mout_fimd1 = {
	.clk		= {
		.name		= "mout_fimd1",
	},
	.sources = &exynos5420_clkset_sclk_fimd1,
	.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 4, .size = 1 },
};

static struct clk *exynos5420_clkset_fimd1_mdnie1_list[] = {
	[0] = &exynos5420_clk_mout_fimd1.clk,
	[1] = &exynos5420_clk_mout_mdnie1.clk,
};

static struct clksrc_sources exynos5420_clkset_fimd1_mdnie1 = {
	.sources	= exynos5420_clkset_fimd1_mdnie1_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_fimd1_mdnie1_list),
};

/* DISP1_BLK */
static struct clksrc_clk exynos5420_clk_sclk_pixel = {
	.clk		= {
		.name		= "sclk_pixel",
	},
	.sources = &exynos5420_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 24, .size = 3 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 28, .size = 4 },
};

static struct clk exynos5420_clk_sclk_hdmiphy = {
	.name		= "sclk_hdmiphy",
	.rate		= 24000000,
};

static struct clk *exynos5420_clkset_sclk_hdmi_list[] = {
	[0] = &exynos5420_clk_sclk_pixel.clk,
	[1] = &exynos5420_clk_sclk_hdmiphy,
};

static struct clksrc_sources exynos5420_clkset_sclk_hdmi = {
	.sources	= exynos5420_clkset_sclk_hdmi_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_sclk_hdmi_list),
};

/* For CLKOUT */
static struct clk *exynos5420_clkset_clk_clkout_list[] = {
	/* Others are for debugging */
	[16] = &clk_xxti,
	[17] = &clk_xusbxti,
};

static struct clksrc_sources exynos5420_clkset_clk_clkout = {
	.sources        = exynos5420_clkset_clk_clkout_list,
	.nr_sources     = ARRAY_SIZE(exynos5420_clkset_clk_clkout_list),
};

static struct clksrc_clk exynos5420_clk_clkout = {
	.clk	= {
		.name		= "clkout",
		.enable         = exynos5_clk_clkout_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos5420_clkset_clk_clkout,
	.reg_src = { .reg = EXYNOS_PMU_DEBUG, .shift = 8, .size = 5 },
};

/* AUDIO_BLK */
static struct clk exynos5420_clk_mau_audioclk = {
	.name		= "mau_audioclk",
	.rate		= 0,
};

static struct clk *exynos5420_clkset_mau_audio0_list[] = {
	[0] = &clk_xtal,
	[1] = &exynos5420_clk_mau_audioclk,
	[2] = &exynos5420_clk_mout_dpll.clk,
	[3] = &exynos5420_clk_mout_mpll.clk,
	[4] = &exynos5420_clk_mout_spll.clk,
	[5] = &exynos5420_clk_mout_ipll.clk,
	[6] = &exynos5420_clk_mout_epll.clk,
	[7] = &exynos5420_clk_mout_rpll.clk,
};

static struct clksrc_sources exynos5420_clkset_mau_audio0 = {
	.sources	= exynos5420_clkset_mau_audio0_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_mau_audio0_list),
};

static struct clksrc_clk exynos5420_clk_sclk_mau_audio0 = {
	.clk		= {
		.name		= "sclk_audio",
		.enable		= exynos5_clksrc_mask_maudio_ctrl,
		.ctrlbit	= (1 << 28),
	},
	.sources	= &exynos5420_clkset_mau_audio0,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_MAUDIO, .shift = 28, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_MAUDIO, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos5420_clk_sclk_mau_pcm0 = {
	.clk		= {
		.name		= "sclk_pcm",
		.parent		= &exynos5420_clk_sclk_mau_audio0.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV_MAUDIO, .shift = 24, .size = 8 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi0_isp = {
	.clk		= {
		.name		= "sclk_spi0_isp",
	},
	.sources	= &exynos5420_clkset_group,
	.reg_src	= { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 12, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi1_isp = {
	.clk		= {
		.name		= "sclk_spi1_isp",
	},
	.sources	= &exynos5420_clkset_group,
	.reg_src	= { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 16, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi0 = {
	.clk		= {
		.name		= "sclk_spi0",
	},
	.sources	= &exynos5420_clkset_group,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 20, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_PERIC1, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi1 = {
	.clk		= {
		.name		= "sclk_spi1",
	},
	.sources	= &exynos5420_clkset_group,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 24, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_PERIC1, .shift = 24, .size = 4 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi2 = {
	.clk		= {
		.name		= "sclk_spi2",
	},
	.sources	= &exynos5420_clkset_group,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 28, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_PERIC1, .shift = 28, .size = 4 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi0_pre = {
	.clk	= {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.0",
		.parent		= &exynos5420_clk_sclk_spi0.clk,
		.enable		= exynos5_clksrc_mask_peric1_ctrl,
		.ctrlbit	= (1 << 20),
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC4, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi1_pre = {
	.clk	= {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.1",
		.parent		= &exynos5420_clk_sclk_spi1.clk,
		.enable		= exynos5_clksrc_mask_peric1_ctrl,
		.ctrlbit	= (1 << 24),
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC4, .shift = 16, .size = 8 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi2_pre = {
	.clk	= {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.2",
		.parent		= &exynos5420_clk_sclk_spi2.clk,
		.enable		= exynos5_clksrc_mask_peric1_ctrl,
		.ctrlbit	= (1 << 28),
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC4, .shift = 24, .size = 8 },
};

static struct clksrc_clk exynos5420_clk_sclk_spi3_pre = {
	.clk	= {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.3",
		.parent		= &exynos5420_clk_sclk_spi0_isp.clk,
	},
	.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 0, .size = 8 },
};

/* audio in PERIC_BLK */
static struct clk exynos5420_clk_audiocdclk0 = {
	.name	= "audiocdclk0",
	.rate	= 0,
};

static struct clk *exynos5420_clkset_dout_audio0_list[] = {
	[0] = &clk_xtal,
	[1] = &exynos5420_clk_audiocdclk0,
	[2] = &exynos5420_clk_mout_dpll.clk,
	[3] = &exynos5420_clk_mout_mpll.clk,
	[4] = &exynos5420_clk_mout_spll.clk,
	[5] = &exynos5420_clk_mout_ipll.clk,
	[6] = &exynos5420_clk_mout_epll.clk,
	[7] = &exynos5420_clk_mout_rpll.clk,
};

static struct clksrc_sources exynos5420_clkset_dout_audio0 = {
	.sources	= exynos5420_clkset_dout_audio0_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_dout_audio0_list),
};

static struct clksrc_clk exynos5420_clk_dout_audio0 = {
	.clk		= {
		.name		= "dout_audio0",
	},
	.sources	= &exynos5420_clkset_dout_audio0,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 8, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_PERIC3, .shift = 20, .size = 4 },
};

static struct clk exynos5420_clk_audiocdclk1 = {
	.name	= "audiocdclk1",
	.rate	= 0,
};

static struct clk *exynos5420_clkset_dout_audio1_list[] = {
	[0] = &clk_xtal,
	[1] = &exynos5420_clk_audiocdclk1,
	[2] = &exynos5420_clk_mout_dpll.clk,
	[3] = &exynos5420_clk_mout_mpll.clk,
	[4] = &exynos5420_clk_mout_spll.clk,
	[5] = &exynos5420_clk_mout_ipll.clk,
	[6] = &exynos5420_clk_mout_epll.clk,
	[7] = &exynos5420_clk_mout_rpll.clk,
};

static struct clksrc_sources exynos5420_clkset_dout_audio1 = {
	.sources	= exynos5420_clkset_dout_audio1_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_dout_audio1_list),
};

static struct clksrc_clk exynos5420_clk_dout_audio1 = {
	.clk		= {
		.name		= "dout_audio1",
	},
	.sources	= &exynos5420_clkset_dout_audio1,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 12, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_PERIC3, .shift = 24, .size = 4 },
};

static struct clk exynos5420_clk_audiocdclk2 = {
	.name	= "audiocdclk2",
	.rate	= 0,
};

static struct clk *exynos5420_clkset_dout_audio2_list[] = {
	[0] = &clk_xtal,
	[1] = &exynos5420_clk_audiocdclk2,
	[2] = &exynos5420_clk_mout_dpll.clk,
	[3] = &exynos5420_clk_mout_mpll.clk,
	[4] = &exynos5420_clk_mout_spll.clk,
	[5] = &exynos5420_clk_mout_ipll.clk,
	[6] = &exynos5420_clk_mout_epll.clk,
	[7] = &exynos5420_clk_mout_rpll.clk,
};

static struct clksrc_sources exynos5420_clkset_dout_audio2 = {
	.sources	= exynos5420_clkset_dout_audio2_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_dout_audio2_list),
};

static struct clksrc_clk exynos5420_clk_dout_audio2 = {
	.clk		= {
		.name		= "dout_audio2",
	},
	.sources	= &exynos5420_clkset_dout_audio2,
	.reg_src	= { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 16, .size = 3 },
	.reg_div	= { .reg = EXYNOS5_CLKDIV_PERIC3, .shift = 28, .size = 4 },
};

static struct clk exynos5420_clk_spdifcdclk = {
	.name	= "spdifcdclk",
};

static struct clk *exynos5420_clkset_sclk_spdif_list[] = {
	[0] = &clk_xtal,
	[1] = &exynos5420_clk_dout_audio0.clk,
	[2] = &exynos5420_clk_dout_audio1.clk,
	[3] = &exynos5420_clk_dout_audio2.clk,
	[4] = &exynos5420_clk_spdifcdclk,
	[5] = &exynos5420_clk_mout_ipll.clk,
	[6] = &exynos5420_clk_mout_epll.clk,
	[7] = &exynos5420_clk_mout_rpll.clk,
};

static struct clksrc_sources exynos5420_clkset_sclk_spdif = {
	.sources	= exynos5420_clkset_sclk_spdif_list,
	.nr_sources	= ARRAY_SIZE(exynos5420_clkset_sclk_spdif_list),
};

static struct clksrc_clk exynos5420_clk_sclk_spdif = {
	.clk	= {
		.name		= "sclk_spdif",
		.enable		= exynos5_clksrc_mask_peric0_ctrl,
		.ctrlbit	= (1 << 28),
	},
	.sources = &exynos5420_clkset_sclk_spdif,
	.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 28, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_aclk_div1 = {
	.clk		= {
		.name		= "aclk_div1",
		.parent		= &exynos5420_aclk_333_432_isp.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV_ISP0, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5420_clk_mscl_blk_div = {
	.clk		= {
		.name		= "mscl_blk_div",
		.parent		= &exynos5420_aclk_400_mscl_sw.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV2_RATIO0, .shift = 28, .size = 2 },
};

static struct clksrc_clk exynos5420_clk_disp1_blk_div = {
	.clk		= {
		.name		= "disp1_blk_div",
		.parent		= &exynos5420_aclk_200_sw.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV2_RATIO0, .shift = 16, .size = 2 },
};

static struct clksrc_clk exynos5420_clk_gscl_blk_div = {
	.clk		= {
		.name		= "gscl_blk_div",
		.parent		= &exynos5420_aclk_333_432_gscl_sw.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV2_RATIO0, .shift = 6, .size = 2 },
};

static struct clksrc_clk exynos5420_clk_gscl_blk_div1 = {
	.clk		= {
		.name		= "gscl_blk_div1",
		.parent		= &exynos5420_aclk_333_432_gscl_sw.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV2_RATIO0, .shift = 4, .size = 2 },
};

static struct clksrc_clk exynos5420_clk_mfc_blk_div = {
	.clk		= {
		.name		= "mfc_blk_div",
		.parent		= &exynos5420_aclk_333_sw.clk,
	},
	.reg_div	= { .reg = EXYNOS5_CLKDIV4_RATIO, .shift = 0, .size = 2 },
};

/* CLOCK GATING LISTS */
static struct clk exynos5420_init_clocks[] = {
	{
		.name		= "uart",
		.devname	= "exynos4210-uart.0",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.1",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.2",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.3",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.4",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "lcd",
		.devname	= "exynos5-fb.1",
		.enable		= exynos5420_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 0) | (1 << 10) | (1 << 11),
	}, {
		.name		= "tmu_apbif",
		.enable		= exynos5_clk_ip_peris_ctrl,
		.ctrlbit	= (1 << 21),
	}, {
		.name		= "mdnie1",
		.enable 	= exynos5420_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 2),
#ifdef CONFIG_S5P_DP
	}, {
		.name		= "dp",
		.devname	= "s5p-dp",
		.enable		= exynos5420_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 4),
#endif
	}, {
		.name		= "clk_ahb2apb_g3dp",
		.enable		= exynos5_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		/* HDMI_MASK should be set 1 before here, */
		/* currently reset value */
		.name           = "pixel_mask",
		.enable         = exynos5420_clksrc_mask_disp1_0_ctrl,
		.ctrlbit        = (1 << 24),
	}, {
		.name           = "mipi1_mask",
		.enable         = exynos5420_clksrc_mask_disp1_0_ctrl,
		.ctrlbit        = (1 << 16),
	}, {
		.name           = "clk_ahb2apb_fsys1p",
		.enable         = exynos5420_clksrc_mask_disp1_0_ctrl,
		.ctrlbit        = (1 << 10),
	}, {
		.name           = "clk_ahb2apb_fsyssp",
		.enable         = exynos5420_clksrc_mask_disp1_0_ctrl,
		.ctrlbit        = (1 << 21),
	}, {
		.name		= "dp",
#ifdef CONFIG_S5P_DP
		.devname	= "s5p-dp",
#endif
		.enable		= exynos5420_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "axi_disp1",
		.enable		= exynos5420_clk_bus_disp1_ctrl,
		.parent		= &exynos5420_aclk_300_disp1.clk,
		.ctrlbit	= (1 << 4) | (0x3 << 23) | (0x3 << 26),
	}
};

static int exynos5_gate_clk_set_parent(struct clk *clk, struct clk *parent)
{
	clk->parent = parent;
	return 0;
}

static struct clk_ops exynos5_gate_clk_ops = {
	.set_parent = exynos5_gate_clk_set_parent
};

/* CLOCK GATING OFF LISTS */
static struct clk exynos5420_init_clocks_off[] = {
	{
		.name		= "clk_abb_apbif",
		.parent		= &exynos5420_aclk_66_psgen.clk,
		.enable		= exynos5_clk_ip_peris_ctrl,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= "watchdog",
		.parent		= &exynos5420_aclk_66_psgen.clk,
		.enable		= exynos5_clk_ip_peris_ctrl,
		.ctrlbit	= (1 << 19),
	}, {
		.name		= "rtc",
		.parent		= &exynos5420_aclk_66_psgen.clk,
		.enable		= exynos5_clk_ip_peris_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "timers",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.0",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.1",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.2",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.3",
		.enable		= &exynos5_clk_ip_isp1_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "dwmci",
		.devname	= "dw_mmc.0",
		.parent		= &exynos5420_aclk_200_fsys2.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "dwmci",
		.devname	= "dw_mmc.1",
		.parent		= &exynos5420_aclk_200_fsys2.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "dwmci",
		.devname	= "dw_mmc.2",
		.parent		= &exynos5420_aclk_200_fsys2.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "usbhost",
		.parent		= &exynos5420_aclk_200_fsys.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "usbdrd30",
		.devname	= "exynos-dwc3.0",
		.parent		= &exynos5420_aclk_200_fsys.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 19) | (1 << 25),
	}, {
		.name		= "usbdrd30",
		.devname	= "exynos-dwc3.1",
		.parent		= &exynos5420_aclk_200_fsys.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 20) | (1 << 26),
	}, {
		.name		= "bts.ufs",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= "ufs",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 23) | (1 << 22),
	}, {
		.name		= "ppmu.ufs",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 28),
	}, {
		.name		= "g3d",
#ifdef CONFIG_MALI_T6XX
		.devname	= "mali.0",
#endif
		.enable		= exynos5_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 9) | (1 << 1),
	}, {
		.name		= "hpm_g3d",
		.enable		= exynos5_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "dsim1",
		.enable		= exynos5420_clk_ip_disp1_ctrl,
		.parent		= &exynos5420_aclk_200_disp1.clk,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "secss",
		.parent		= &exynos5420_aclk_266_g2d.clk,
		.enable		= exynos5_clk_ip_g2d_ctrl,
		.ctrlbit	= (1 << 2) | (1 << 9),
	}, {
		.name		= "slimsss",
		.parent		= &exynos5420_aclk_266_g2d.clk,
		.enable		= exynos5_clk_ip_g2d_ctrl,
		.ctrlbit	= (1 << 12) | (1 << 14),
	}, {
		.name		= "mfc",
#ifdef CONFIG_S5P_DEV_MFC
		.devname	= "s3c-mfc",
#endif
		.enable		= exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= ((3 << 3) | (1 << 0)),
	}, {
		.name		= "jpeg-hx",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 12) | (1 << 2),
	}, {
		.name		= "jpeg2-hx",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "hdmicec",
		.parent		= &exynos5420_aclk_66_dout.clk,
		.enable		= exynos5_clk_ip_peris_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "hdmi",
#ifdef CONFIG_S5P_DEV_TV
		.devname	= "exynos5-hdmi",
#endif
		.parent		= &exynos5420_aclk_66_dout.clk,
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "mixer",
#ifdef CONFIG_S5P_DEV_TV
		.devname	= "s5p-mixer",
#endif
		.parent		= &exynos5420_aclk_200_disp1.clk,
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= ((1 << 5) | (1 << 13) | (1 << 14)),
	}, {
		.name		= "bts.mfc",
		.enable		= exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= (0x3 << 3),
	}, {
		.name		= "ppmu.mfc",
		.enable		= exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= (0x3 << 5),
	}, {
		.name		= "bts.isp0",
		.enable		= exynos5_clk_ip_isp0_ctrl,
		.ctrlbit	= (0x3f << 14),
	}, {
		.name		= "ppmu.isp0",
		.enable		= exynos5_clk_ip_isp0_ctrl,
		.ctrlbit	= (0x3 << 20),
	}, {
		.name		= "bts.gscl0",
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.ctrlbit	= (0x3 << 28),
	}, {
		.name		= "ppmu.gscl0",
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.ctrlbit	= (0xF << 19),
	}, {
		.name		= "bts.disp1",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= ((0x3 << 13) | (0x3 << 10)),
	}, {
		.name		= "ppmu.disp1",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (0x3 << 17),
	}, {
		.name		= "bts.g3d",
		.enable		= exynos5_clk_ip_g3d_ctrl,
		.ctrlbit	= (0x1 << 1),
	}, {
		.name		= "bts.gen",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (0x3 << 11),
	}, {
		.name		= "ppmu.gen",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (0x1 << 15),
	}, {
		.name		= "bts.fsys",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= ((0x3 << 25) | (0x1 << 22)),
	}, {
		.name		= "ppmu.fsys",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (0x1 << 28),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mfc_lr, 0),
		.enable		= &exynos5_clk_ip_mfc_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(tv, 2),
		.enable		= &exynos5_clk_ip_disp1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(rot, 4),
		.enable		= &exynos5_clk_ip_gen_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(gsc0, 5),
		.enable		= &exynos5_clk_ip_gscl1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(gsc1, 6),
		.enable		= &exynos5_clk_ip_gscl1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp0, 9),
		.enable		= &exynos5_clk_ip_isp0_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd1, 11),
		.enable		= &exynos5_clk_ip_disp1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd1a, 30),
		.enable		= &exynos5_clk_ip_disp1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif0, 12),
		.enable		= &exynos5_clk_ip_gscl1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif1, 13),
		.enable		= &exynos5_clk_ip_gscl1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif2, 14),
		.enable		= &exynos5_clk_ip_gscl1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(2d, 15),
		.enable		= &exynos5_clk_ip_g2d_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp1, 16),
		.enable		= &exynos5_clk_ip_isp1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp2, 17),
		.enable		= &exynos5_clk_ip_isp0_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		/* parent of "jpeg-hx" and "jpeg2-hx" */
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(jpeg, 3),
		.parent		= &exynos5420_aclk_300_jpeg.clk,
		.enable		= &exynos5_clk_ip_gen_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mjpeg, 20),
		.enable		= &exynos5_clk_ip_gen_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0, /* gated by (jpeg, 3) */
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mjpeg2, 29),
		.enable		= &exynos5_clk_ip_gen_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0, /* gated by (jpeg, 3) */
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp3, 21),
		.enable		= &exynos5_clk_ip_gscl1_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(scaler0r, 23),
		.enable		= &exynos5_clk_ip_mscl_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(scaler1r, 25),
		.enable		= &exynos5_clk_ip_mscl_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(scaler2r, 27),
		.enable		= &exynos5_clk_ip_mscl_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= 0,
	}, {
		.name		= "mscl",
#ifdef CONFIG_EXYNOS5_DEV_SCALER
		.devname	= "exynos5-scaler.0",
#endif
		.parent		= &exynos5420_aclk_400_mscl.clk,
		.enable		= exynos5_clk_ip_mscl_ctrl,
		.ctrlbit	= (1 << 0) | (1 << 5) | (1 << 6) | (1 << 7),
	}, {
		.name		= "mscl",
#ifdef CONFIG_EXYNOS5_DEV_SCALER
		.devname	= "exynos5-scaler.1",
#endif
		.parent		= &exynos5420_aclk_400_mscl.clk,
		.enable		= exynos5_clk_ip_mscl_ctrl,
		.ctrlbit	= (1 << 1) | (1 << 6),
	}, {
		.name		= "mscl",
#ifdef CONFIG_EXYNOS5_DEV_SCALER
		.devname	= "exynos5-scaler.2",
#endif
		.parent		= &exynos5420_aclk_400_mscl.clk,
		.enable		= exynos5_clk_ip_mscl_ctrl,
		.ctrlbit	= (1 << 2) | (1 << 7),
	}, {
		.name		= "gscl",
#ifdef CONFIG_EXYNOS5_DEV_GSC
		.devname	= "exynos-gsc.0",
#endif
		.parent		= &exynos5420_aclk_300_gscl.clk,
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.ctrlbit	= ((1 << 28) | (1 << 14) | (1 << 0)),
	}, {
		.name		= "gscl",
#ifdef CONFIG_EXYNOS5_DEV_GSC
		.devname	= "exynos-gsc.1",
#endif
		.parent		= &exynos5420_aclk_300_gscl.clk,
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.ctrlbit	= ((1 << 29) | (1 << 15) | (1 << 1)),
	}, {
		.name		= "gscl_flite0",
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.parent		= &exynos5420_aclk_333_432_gscl.clk,
		.ctrlbit	= ((1 << 20) | (1 << 10) | (1 << 5)),
	}, {
		.name		= "gscl_flite1",
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.parent		= &exynos5420_aclk_333_432_gscl.clk,
		.ctrlbit	= ((1 << 11) | (1 << 6)),
	}, {
		.name		= "gscl0_flite2",
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.parent		= &exynos5420_aclk_333_432_gscl.clk,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "gscl_3aa",
		.enable		= exynos5_clk_ip_gscl0_ctrl,
		.ctrlbit	= ((1 << 19) | (1 << 9) | (1 << 4)),
	}, {
		.name		= "gscl1_flite2",
		.enable		= exynos5_clk_ip_gscl1_ctrl,
		.parent		= &exynos5420_aclk_333_432_gscl.clk,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "gscl_wrap0",
		.devname	= "s5p-mipi-csis.0",
		.enable		= exynos5_clk_ip_gscl1_ctrl,
		.ctrlbit	= (1 << 18) | (1 << 12),
	}, {
		.name		= "gscl_wrap1",
		.devname	= "s5p-mipi-csis.1",
		.enable		= exynos5_clk_ip_gscl1_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "isp0_333_432",
		.devname	= "exynos5-fimc-is",
		.enable		= exynos5_clk_ip_isp0_ctrl,
		.ctrlbit	= ((0x3FFFF << 14) | (0x1F << 0)),
	}, {
		.name		= "isp0_400",
		.devname	= "exynos5-fimc-is",
		.enable 	= exynos5_clk_ip_isp0_ctrl,
		.ctrlbit	= (0x7 << 5),
	}, {
		.name		= "isp0_266",
		.devname	= "exynos5-fimc-is",
		.enable		= exynos5_clk_ip_isp0_ctrl,
		.ctrlbit	= ((1 << 3) | (1 << 1)),
	}, {
		.name		= "isp1_333_432",
		.devname	= "exynos5-fimc-is",
		.enable		= exynos5_clk_ip_isp1_ctrl,
		.ctrlbit	= ((0x3F << 17) | (0x3 << 12)),
	}, {
		.name		= "gate_ip_dis",
		.devname	= "exynos5-fimc-is",
		.enable		= exynos5_clk_ip_isp1_ctrl,
		.ctrlbit	= ((0x3 << 20) | (0x3 << 9) | (0x3 << 5) | (1 << 1)),
	}, {
		.name		= "gate_ip_tdnr",
		.devname	= "exynos5-fimc-is",
		.enable		= exynos5_clk_ip_isp1_ctrl,
		.ctrlbit	= ((1 << 22) | (1 << 11) | (1 << 7) | (1 << 2)),
	}, {
		.name		= "isp1_266",
		.devname	= "exynos5-fimc-is",
		.enable		= exynos5_clk_ip_isp1_ctrl,
		.ctrlbit	= ((0xF << 8) | (0x7 << 0)),
	}, {
		.name		= "fimg2d",
		.devname	= "s5p-fimg2d",
		.enable		= exynos5_clk_ip_g2d_ctrl,
		.ctrlbit	= ((1 << 3) | (1 << 10)),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.1",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.2",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 21),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.1",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.2",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= "spdif",
		.devname	= "samsung-spdif",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 26),
#ifndef CONFIG_S5P_DP
	}, {
		.name		= "dp",
#ifdef CONFIG_S5P_DP
		.devname	= "s5p-dp",
#endif
		.enable		= exynos5420_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 4),
#endif
	}, {
		.name		= "mdma1",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 4) | (1 << 14),
	}, {
		.name		= "rotator",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 1) | (1 << 11),
	}, {
		.name		= "sysmmu-mdma-ssss",
		.enable		= exynos5_clk_ip_g2d_ctrl,
		.ctrlbit	= (1 << 5) | (1 << 13) | (1 << 6),
	}, {
		.name		= "sysmmu-mdma1",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "top_rtc",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "sysmmu-rtic",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "asyncxim_gscl",
		.enable		= exynos5420_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 19),
	}, {
		.name		= "ctl_rtic",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "clkm_phy",
		.enable		= exynos5_clk_bus_cdrex_ctrl,
		.ctrlbit	= ((1 << 1) | (1 << 0)),
	}, {
		.name		= "gate_sclk_isp_sensorx",
		.enable		= exynos5_clksrc_mask_isp_sensor_ctrl,
		.ctrlbit	= ((1 << 4) | (1 << 8) | (1 << 12)),
	},
};

static struct clksrc_clk exynos5420_clksrcs_off[] = {
	{
		.clk	= {
			.name		= "sclk_hdmi",
			.enable		= exynos5420_clksrc_mask_disp1_0_ctrl,
			.ctrlbit	= (1 << 28),
		},
		.sources = &exynos5420_clkset_sclk_hdmi,
		.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 28, .size = 1},
	}, {
		.clk	= {
			.name		= "sclk_dp1_ext_mst_vid",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 20, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 24, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mphy_refclk",
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 28)
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 28, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS2, .shift = 16, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_unipro",
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 24)
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 24, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS2, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mipi1",
			.enable		= exynos5420_clksrc_mask_disp1_0_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 16, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 16, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mdnie_pwm1",
			.enable		= exynos5420_clksrc_mask_disp1_0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 12, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_usbdrd30",
			.devname	= "exynos-dwc3.1",
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 4)
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 4, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS0, .shift = 20, .size = 4 },
	},
};

static struct clksrc_clk exynos5420_clksrcs[] = {
	{
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.0",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 4, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 8, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.1",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 8, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.2",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 12, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 16, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.3",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 16, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 20, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.4",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 20),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 20, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 24, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc.0",
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 8)
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 8, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 0, .size = 10 },
	}, {
		.clk	= {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc.1",
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 12)
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 12, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 10, .size = 10 },
	}, {
		.clk	= {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc.2",
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 16)
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 16, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 20, .size = 10 },
	}, {
		.clk	= {
			.name		= "sclk_fimd",
			.enable		= exynos5420_clksrc_mask_disp1_0_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos5420_clkset_fimd1_mdnie1,
		.reg_src = { .reg = EXYNOS5_CLKSRC_CMUTOP_SPARE2, .shift = 8, .size = 1 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mdnie",
			.parent		= &exynos5420_clk_mout_mdnie1.clk,
			.enable		= exynos5420_clksrc_mask_disp1_0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 4, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_usbdrd30",
			.devname		= "exynos-dwc3.0",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 20, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS0, .shift = 24, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_usbphy30",
			.devname		= "exynos-dwc3.0",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 20, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS0, .shift = 16, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_usbphy30",
			.devname		= "exynos-dwc3.1",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 4, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS0, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_hsic_12m",
			.parent		=  &clk_ext_xtal_mux,
		},
	}, {
		.clk	= {
			.name		= "sclk_isp_sensor0",
			.enable		= exynos5_clksrc_mask_isp_sensor_ctrl,
			.ctrlbit		= (1 << 4),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 28, .size = 3 },
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_isp_sensor1",
			.enable		= exynos5_clksrc_mask_isp_sensor_ctrl,
			.ctrlbit		= (1 << 8),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 28, .size = 3 },
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP, .shift = 16, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_isp_sensor2",
			.enable		= exynos5_clksrc_mask_isp_sensor_ctrl,
			.ctrlbit		= (1 << 12),
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 28, .size = 3 },
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pwm_isp",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 24, .size = 3 },
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 28, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_uart_isp",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_SCLK_SRC_ISP, .shift = 20, .size = 3 },
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 24, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_spi0_isp_pre",
			.parent		= &exynos5420_clk_sclk_spi0_isp.clk,
		},
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 0, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_spi1_isp_pre",
			.parent		= &exynos5420_clk_sclk_spi0_isp.clk,
		},
		.reg_div = { .reg = EXYNOS5_SCLK_DIV_ISP1, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pwm",
		},
		.sources = &exynos5420_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 24, .size = 3 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 28, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_pcm1",
			.parent		= &exynos5420_clk_dout_audio1.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC2, .shift = 16, .size = 8 },
	}, {
		.clk	= {
			.name		= "scok_i2s1",
			.parent		= &exynos5420_clk_dout_audio1.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC3, .shift = 6, .size = 6 },
	}, {
		.clk	= {
			.name		= "sclk_pcm2",
			.parent		= &exynos5420_clk_dout_audio2.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC2, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "scok_i2s2",
			.parent		= &exynos5420_clk_dout_audio2.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC3, .shift = 12, .size = 6 },
	}, {
		.clk	= {
			.name		= "pclk_acp",
			.parent		= &exynos5420_aclk_266_g2d.clk,
		},
		.reg_div = { .reg = EXYNOS5410_CLKDIV_G2D, .shift = 4, .size = 3 },
	}, {
		.clk	= {
			.name		= "aclk_div0",
			.parent		= &exynos5420_aclk_333_432_isp.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_ISP0, .shift = 0, .size = 3 },
	}, {
		.clk	= {
			.name		= "aclk_div2",
			.parent		= &exynos5420_clk_aclk_div1.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_ISP2, .shift = 0, .size = 3 },
	}, {
		.clk	= {
			.name		= "aclk_mcuisp_div0",
			.parent		= &exynos5420_aclk_400_isp.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_ISP1, .shift = 0, .size = 3 },
	}, {
		.clk	= {
			.name		= "aclk_mcuisp_div1",
			.parent		= &exynos5420_aclk_400_isp.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_ISP1, .shift = 4, .size = 3 },
	}, {
		.clk	= {
			.name		= "pclk_drex0",
			.parent		= &exynos5420_clk_cclk_drex0.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 28, .size = 3 },
	}, {
		.clk	= {
			.name		= "pclk_drex1",
			.parent		= &exynos5420_clk_cclk_drex1.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 28, .size = 3 },
	}, {
		.clk	= {
			.name		= "pclk_cdrex",
			.parent		= &exynos5420_clk_aclk_cdrex1.clk,
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX0, .shift = 28, .size = 3 },
	},
};

static unsigned long exynos5_pll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos5_pll2550_set_rate(struct clk *clk, struct pll_2550 *pll, unsigned long rate)
{
	unsigned int pll_con0;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	pll_con0 = __raw_readl(pll->pll_con0);
	pll_con0 &= ~(PLL2650_MDIV_MASK << PLL2650_MDIV_SHIFT |
			PLL2650_PDIV_MASK << PLL2650_PDIV_SHIFT |
			PLL2650_SDIV_MASK << PLL2650_SDIV_SHIFT);

	for (i = 0; i < pll->pms_cnt; i++) {
		if (pll->pms_div[i].rate == rate) {
			pll_con0 |= pll->pms_div[i].pdiv << PLL2650_PDIV_SHIFT;
			pll_con0 |= pll->pms_div[i].mdiv << PLL2650_MDIV_SHIFT;
			pll_con0 |= pll->pms_div[i].sdiv << PLL2650_SDIV_SHIFT;
			pll_con0 |= 1 << EXYNOS5_PLL_ENABLE_SHIFT;
			break;
		}
	}

	if (i == pll->pms_cnt) {
		pr_err("%s: Invalid Clock %s Frequency\n", __func__, clk->name);
		return -EINVAL;
	}

	/* 200 max_cycle : specification data */
	locktime = 200 * pll->pms_div[i].pdiv;

	__raw_writel(locktime, pll->pll_lock);
	__raw_writel(pll_con0, pll->pll_con0);

	do {
		tmp = __raw_readl(pll->pll_con0);
	} while (!(tmp & (0x1 << EXYNOS5_PLLCON0_LOCKED_SHIFT)));

	clk->rate = rate;

	return 0;
}

static int exynos5_pll2650_set_rate(struct clk *clk, struct pll_2650 *pll, unsigned long rate)
{
	unsigned int pll_con0, pll_con1;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;
	unsigned int k;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	pll_con0 = __raw_readl(pll->pll_con0);
	pll_con0 &= ~(PLL2650_MDIV_MASK << PLL2650_MDIV_SHIFT |
			PLL2650_PDIV_MASK << PLL2650_PDIV_SHIFT |
			PLL2650_SDIV_MASK << PLL2650_SDIV_SHIFT);

	pll_con1 = __raw_readl(pll->pll_con1);
	pll_con1 &= ~(0xffff << 0);

	for (i = 0; i < pll->pms_cnt; i++) {
		if (pll->pms_div[i].rate == rate) {
			k = (~(pll->pms_div[i].k) + 1) & pll->pllcon1_k_mask;
			pll_con1 |= k << 0;
			pll_con0 |= pll->pms_div[i].pdiv << PLL2650_PDIV_SHIFT;
			pll_con0 |= pll->pms_div[i].mdiv << PLL2650_MDIV_SHIFT;
			pll_con0 |= pll->pms_div[i].sdiv << PLL2650_SDIV_SHIFT;
			pll_con0 |= 1 << EXYNOS5_PLL_ENABLE_SHIFT;
			break;
		}
	}

	if (i == pll->pms_cnt) {
		pr_err("%s: Invalid Clock %s Frequency\n", __func__, clk->name);
		return -EINVAL;
	}

	/* 3000 max_cycle : specification data */
	locktime = 500 * pll->pms_div[i].pdiv;

	__raw_writel(locktime, pll->pll_lock);
	__raw_writel(pll_con0, pll->pll_con0);
	__raw_writel(pll_con1, pll->pll_con1);

	do {
		tmp = __raw_readl(pll->pll_con0);
	} while (!(tmp & (0x1 << EXYNOS5_PLLCON0_LOCKED_SHIFT)));

	clk->rate = rate;

	return 0;
}

/* CPLL */
static struct pll_div_data exynos5_cpll_div[] = {
	{666000000, 4, 222, 1, 0, 0, 0},
	{640000000, 3, 160, 1, 0, 0, 0},
	{320000000, 3, 160, 2, 0, 0, 0},
};

PLL_2550(cpll_data, CPLL, exynos5_cpll_div);

static int exynos5420_cpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2550_set_rate(clk, &cpll_data, rate);
}

static struct clk_ops exynos5420_cpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_cpll_set_rate,
};

/* EPLL */
static struct vpll_div_data exynos5_epll_div[] = {
	{ 45158400, 3, 181, 5, 24012, 0, 0, 0},
	{ 49152000, 3, 197, 5, 25690, 0, 0, 0},
	{ 67737600, 5, 452, 5, 27263, 0, 0, 0},
	{180633600, 5, 301, 3,  3670, 0, 0, 0},
	{100000000, 3, 200, 4,     0, 0, 0, 0},
	{200000000, 3, 200, 3,     0, 0, 0, 0},
	{400000000, 3, 200, 2,     0, 0, 0, 0},
	{600000000, 2, 100, 1,     0, 0, 0, 0},
};

PLL_2650(epll_data, EPLL, exynos5_epll_div);

static int exynos5420_epll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2650_set_rate(clk, &epll_data, rate);
}

static struct clk_ops exynos5420_epll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_epll_set_rate,
};

/* DPLL */
static struct pll_div_data exynos5_dpll_div[] = {
	{600000000, 2, 100, 1, 0,  0, 0},
};

PLL_2550(dpll_data, DPLL, exynos5_dpll_div);

static int exynos5420_dpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2550_set_rate(clk, &dpll_data, rate);
}

static struct clk_ops exynos5420_dpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_dpll_set_rate,
};

/* IPLL */
static struct pll_div_data exynos5_ipll_div[] = {
	{370000000, 3, 185, 2, 0,  0, 0},
	{432000000, 4, 288, 2, 0,  0, 0},
	{666000000, 4, 222, 1, 0,  0, 0},
	{864000000, 4, 288, 1, 0,  0, 0},
};

PLL_2550(ipll_data, IPLL, exynos5_ipll_div);

static int exynos5420_ipll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2550_set_rate(clk, &ipll_data, rate);
}

static struct clk_ops exynos5420_ipll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_ipll_set_rate,
};

/* VPLL */
static struct pll_div_data exynos5_vpll_div[] = {
	{100000000, 3, 200, 4, 0,  0, 0},
	{177000000, 2, 118, 3, 0,  0, 0},
	{266000000, 3, 266, 3, 0,  0, 0},
	{350000000, 3, 175, 2, 0,  0, 0},
	{420000000, 2, 140, 2, 0,  0, 0},
	{480000000, 2, 160, 2, 0,  0, 0},
	{533000000, 3, 266, 2, 0,  0, 0},
	{600000000, 2, 200, 2, 0,  0, 0},
};

PLL_2550(vpll_data, VPLL, exynos5_vpll_div);

static int exynos5420_vpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2550_set_rate(clk, &vpll_data, rate);
}

static struct clk_ops exynos5420_vpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_vpll_set_rate,
};

/* SPLL */
static struct pll_div_data exynos5_spll_div[] = {
	{66000000, 4, 352, 5, 0,  0, 0},
	{400000000, 3, 200, 2, 0,  0, 0},
};

PLL_2550(spll_data, SPLL, exynos5_spll_div);

static int exynos5420_spll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2550_set_rate(clk, &spll_data, rate);
}

static struct clk_ops exynos5420_spll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_spll_set_rate,
};

/* MPLL */
static struct pll_div_data exynos5_mpll_div[] = {
	{532000000, 3, 266, 2, 0,  0, 0},
};

PLL_2550(mpll_data, MPLL, exynos5_mpll_div);

static int exynos5420_mpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2550_set_rate(clk, &mpll_data, rate);
}

static struct clk_ops exynos5420_mpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_mpll_set_rate,
};

/* RPLL */
static struct vpll_div_data exynos5_rpll_div[] = {
	{133000000, 3, 133, 3,     0, 0, 0, 0},
	{266000000, 3, 133, 2,     0, 0, 0, 0},
	{300000000, 2, 100, 2,     0, 0, 0, 0},
};

PLL_2650(rpll_data, RPLL, exynos5_rpll_div);

static int exynos5420_rpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5_pll2650_set_rate(clk, &rpll_data, rate);
}

static struct clk_ops exynos5420_rpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_rpll_set_rate,
};

/* BPLL */
static struct pll_div_data exynos5_bpll_div[] = {
	{933000000,  4, 311, 1, 0, 0, 0},
	{800000000,  3, 200, 1, 0, 0, 0},
	{733000000,  2, 122, 1, 0, 0, 0},
	{667000000,  2, 111, 1, 0, 0, 0},
	{533000000,  3, 266, 2, 0, 0, 0},
	{400000000,  3, 200, 2, 0, 0, 0},
	{266000000,  3, 266, 3, 0, 0, 0},
	{200000000,  3, 200, 3, 0, 0, 0},
	{160000000,  3, 160, 3, 0, 0, 0},
	{133000000,  3, 266, 4, 0, 0, 0},
};
PLL_2550(bpll_data, BPLL, exynos5_bpll_div);

static int exynos5420_bpll_set_rate(struct clk *clk, unsigned long rate)
{
	/* Change frequency unit to Hz */
	rate = rate * 1000;

	return exynos5_pll2550_set_rate(clk, &bpll_data, rate);
}

static struct clk_ops exynos5420_bpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5420_bpll_set_rate,
};

/* For APLL and KPLL */
static int exynos5420_fout_simple_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos5420_fout_simple_ops = {
	.set_rate = exynos5420_fout_simple_set_rate
};

/* Clock initialization code */
static struct clksrc_clk *exynos5420_sysclks[] = {
	&exynos5420_clk_fout_ipll,
	&exynos5420_clk_mout_apll,
	&exynos5420_clk_sclk_apll,
	&exynos5420_clk_mout_mpll,
	&exynos5420_clk_mout_bpll,
	&exynos5420_clk_sclk_bpll,
	&exynos5420_clk_mout_dpll,
	&exynos5420_clk_mout_ipll,
	&exynos5420_clk_mout_epll,
	&exynos5420_clk_mout_cpll,
	&exynos5420_clk_mout_rpll,
	&exynos5420_clk_mout_vpll,
	&exynos5420_clk_mout_spll,
	&exynos5420_clk_mout_cpu,
	&exynos5420_clk_armclk,
	&exynos5420_clk_aclk_cpud,
	&exynos5420_clk_pclk_dbg,
	&exynos5420_clk_atclk,
	&exynos5420_clk_mout_hpm_cpu,
	&exynos5420_clk_dout_copy,
	&exynos5420_clk_sclk_hpm,
	&exynos5420_clk_mout_kpll,
	&exynos5420_clk_sclk_kpll,
	&exynos5420_clk_mout_kfc,
	&exynos5420_clk_kfc_armclk,
	&exynos5420_clk_aclk_kfc,
	&exynos5420_clk_pclk_kfc,
	&exynos5420_clk_mout_hpm_kfc,
	&exynos5420_clk_sclk_hpm_kfc,
	&exynos5420_clk_mclk_cdrex,
	&exynos5420_clk_sclk_cdrex,
	&exynos5420_clk_cclk_drex0,
	&exynos5420_clk_cclk_drex1,
	&exynos5420_clk_clk2x_phy0,
	&exynos5420_clk_aclk_cdrex1,
	&exynos5420_aclk_200_fsys_dout,
	&exynos5420_aclk_200_fsys_sw,
	&exynos5420_aclk_200_fsys,
	&exynos5420_pclk_200_fsys_dout,
	&exynos5420_pclk_200_fsys_sw,
	&exynos5420_pclk_200_fsys,
	&exynos5420_aclk_100_noc_dout,
	&exynos5420_aclk_100_noc_sw,
	&exynos5420_aclk_100_noc,
	&exynos5420_aclk_400_wcore_mout,
	&exynos5420_aclk_400_wcore_dout,
	&exynos5420_aclk_400_wcore_sw,
	&exynos5420_aclk_400_wcore,
	&exynos5420_aclk_200_fsys2_dout,
	&exynos5420_aclk_200_fsys2_sw,
	&exynos5420_aclk_200_fsys2,
	&exynos5420_aclk_200_dout,
	&exynos5420_aclk_200_sw,
	&exynos5420_aclk_200_disp1,
	&exynos5420_aclk_400_mscl_dout,
	&exynos5420_aclk_400_mscl_sw,
	&exynos5420_aclk_400_mscl,
	&exynos5420_aclk_400_isp_dout,
	&exynos5420_aclk_400_isp_sw,
	&exynos5420_aclk_400_isp,
	&exynos5420_aclk_333_dout,
	&exynos5420_aclk_333_sw,
	&exynos5420_aclk_333,
	&exynos5420_aclk_166_dout,
	&exynos5420_aclk_166_sw,
	&exynos5420_aclk_166,
	&exynos5420_aclk_266_dout,
	&exynos5420_aclk_266_sw,
	&exynos5420_aclk_266,
	&exynos5420_aclk_266_isp,
	&exynos5420_aclk_66_dout,
	&exynos5420_aclk_66_sw,
	&exynos5420_aclk_66_peric,
	&exynos5420_aclk_66_psgen,
	&exynos5420_aclk_66_gpio,
	&exynos5420_aclk_333_432_isp0_dout,
	&exynos5420_aclk_333_432_isp0_sw,
	&exynos5420_aclk_333_432_isp0,
	&exynos5420_aclk_333_432_isp_dout,
	&exynos5420_aclk_333_432_isp_sw,
	&exynos5420_aclk_333_432_isp,
	&exynos5420_aclk_333_432_gscl_dout,
	&exynos5420_aclk_333_432_gscl_sw,
	&exynos5420_aclk_333_432_gscl,
	&exynos5420_aclk_300_gscl_dout,
	&exynos5420_aclk_300_gscl_sw,
	&exynos5420_aclk_300_gscl,
	&exynos5420_aclk_300_disp1_dout,
	&exynos5420_aclk_300_disp1_sw,
	&exynos5420_aclk_300_disp1,
	&exynos5420_aclk_400_disp1_dout,
	&exynos5420_aclk_400_disp1_sw,
	&exynos5420_aclk_400_disp1,
	&exynos5420_aclk_300_jpeg_dout,
	&exynos5420_aclk_300_jpeg_sw,
	&exynos5420_aclk_300_jpeg,
	&exynos5420_aclk_g3d_dout,
	&exynos5420_aclk_g3d_sw,
	&exynos5420_aclk_g3d,
	&exynos5420_aclk_266_g2d_dout,
	&exynos5420_aclk_266_g2d_sw,
	&exynos5420_aclk_266_g2d,
	&exynos5420_aclk_333_g2d_dout,
	&exynos5420_aclk_333_g2d_sw,
	&exynos5420_aclk_333_g2d,
	&exynos5420_mau_epll_clk,
	&exynos5420_mx_mspll_ccore,
	&exynos5420_mx_mspll_cpu,
	&exynos5420_mx_mspll_kfc,
	&exynos5420_clk_mout_mdnie1,
	&exynos5420_clk_mout_fimd1,
	&exynos5420_clk_sclk_mau_audio0,
	&exynos5420_clk_sclk_mau_pcm0,
	&exynos5420_clk_sclk_pixel,
	&exynos5420_clk_sclk_spi0_isp,
	&exynos5420_clk_sclk_spi1_isp,
	&exynos5420_clk_dout_audio0,
	&exynos5420_clk_dout_audio1,
	&exynos5420_clk_dout_audio2,
	&exynos5420_clk_aclk_div1,
	&exynos5420_clk_mscl_blk_div,
	&exynos5420_clk_disp1_blk_div,
	&exynos5420_clk_gscl_blk_div,
	&exynos5420_clk_gscl_blk_div1,
	&exynos5420_clk_mfc_blk_div,
	&exynos5420_clk_clkout,
};

static struct clksrc_clk *exynos5420_sysclks_off[] = {
	&exynos5420_clk_sclk_spi0,
	&exynos5420_clk_sclk_spi1,
	&exynos5420_clk_sclk_spi2,
	&exynos5420_clk_sclk_spi0_pre,
	&exynos5420_clk_sclk_spi1_pre,
	&exynos5420_clk_sclk_spi2_pre,
	&exynos5420_clk_sclk_spi3_pre,
	&exynos5420_clk_sclk_spdif,
};

static struct clk exynos5_i2cs_clocks[] = {
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.0",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.1",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.2",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.3",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.0",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 10)
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.1",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 11)
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.2",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 12)
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.3",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 13)
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-hdmiphy-i2c",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 14)
	}, {
		.name		= "adc",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 15),
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.4",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 28)
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.5",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 30)
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.6",
		.parent		= &exynos5420_aclk_66_peric.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 31)
	}
};

static struct clk *exynos5420_clks[] __initdata = {
	&clk_fout_kpll,
	&clk_ipll,
	&clk_fout_rpll,
	&clk_fout_spll,
	&exynos5420_clk_sclk_hdmiphy,
	&clk_fout_bpll,
	&clk_fout_cpll,
	&clk_v_epll,
	&clk_v_vpll,
	&clk_v_ipll,
};

static struct clk *exynos5_clk_cdev[] = {
	&exynos5_clk_pdma0,
	&exynos5_clk_pdma1,
	&exynos5_clk_mdma,
	&exynos5_clk_adma0,
};

static struct clk_lookup exynos5420_clk_lookup[] = {
	CLKDEV_INIT("exynos4210-uart.0", "clk_uart_baud0", &exynos5420_clksrcs[0].clk),
	CLKDEV_INIT("exynos4210-uart.1", "clk_uart_baud0", &exynos5420_clksrcs[1].clk),
	CLKDEV_INIT("exynos4210-uart.2", "clk_uart_baud0", &exynos5420_clksrcs[2].clk),
	CLKDEV_INIT("exynos4210-uart.3", "clk_uart_baud0", &exynos5420_clksrcs[3].clk),
	CLKDEV_INIT("dma-pl330.0", "apb_pclk", &exynos5_clk_pdma0),
	CLKDEV_INIT("dma-pl330.1", "apb_pclk", &exynos5_clk_pdma1),
	CLKDEV_INIT("dma-pl330.2", "apb_pclk", &exynos5_clk_mdma),
	CLKDEV_INIT("dma-pl330.3", "apb_pclk", &exynos5_clk_adma0),
};

#ifdef CONFIG_PM
static int exynos5420_clock_suspend(void)
{
	s3c_pm_do_save(exynos5420_clock_save, ARRAY_SIZE(exynos5420_clock_save));
	s3c_pm_do_save(exynos5420_cpll_save, ARRAY_SIZE(exynos5420_cpll_save));
	s3c_pm_do_save(exynos5420_dpll_save, ARRAY_SIZE(exynos5420_dpll_save));
	s3c_pm_do_save(exynos5420_ipll_save, ARRAY_SIZE(exynos5420_ipll_save));
	s3c_pm_do_save(exynos5420_epll_save, ARRAY_SIZE(exynos5420_epll_save));
	s3c_pm_do_save(exynos5420_vpll_save, ARRAY_SIZE(exynos5420_vpll_save));
	s3c_pm_do_save(exynos5420_rpll_save, ARRAY_SIZE(exynos5420_rpll_save));
	s3c_pm_do_save(exynos5420_spll_save, ARRAY_SIZE(exynos5420_spll_save));
	s3c_pm_do_save(exynos5420_mpll_save, ARRAY_SIZE(exynos5420_mpll_save));

	return 0;
}

static void exynos5_pll_wait_locktime(void __iomem *con_reg, int shift_value)
{
	unsigned int tmp;

	do {
		tmp = __raw_readl(con_reg);
	} while (tmp >> EXYNOS5_PLL_ENABLE_SHIFT && !(tmp & (0x1 << EXYNOS5_PLLCON0_LOCKED_SHIFT)));
}

static void exynos5420_clock_resume(void)
{
	s3c_pm_do_restore_core(exynos5420_clock_save, ARRAY_SIZE(exynos5420_clock_save));

	s3c_pm_do_restore_core(exynos5420_cpll_save, ARRAY_SIZE(exynos5420_cpll_save));
	s3c_pm_do_restore_core(exynos5420_dpll_save, ARRAY_SIZE(exynos5420_dpll_save));
	s3c_pm_do_restore_core(exynos5420_ipll_save, ARRAY_SIZE(exynos5420_ipll_save));
	s3c_pm_do_restore_core(exynos5420_epll_save, ARRAY_SIZE(exynos5420_epll_save));
	s3c_pm_do_restore_core(exynos5420_vpll_save, ARRAY_SIZE(exynos5420_vpll_save));
	s3c_pm_do_restore_core(exynos5420_rpll_save, ARRAY_SIZE(exynos5420_rpll_save));
	s3c_pm_do_restore_core(exynos5420_spll_save, ARRAY_SIZE(exynos5420_spll_save));
	s3c_pm_do_restore_core(exynos5420_mpll_save, ARRAY_SIZE(exynos5420_mpll_save));

	exynos5_pll_wait_locktime(EXYNOS5_CPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_DPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_IPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_EPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_VPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_RPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_SPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5_MPLL_CON0, EXYNOS5_PLLCON0_LOCKED_SHIFT);

	clk_fout_apll.rate = s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS5_APLL_CON0));
	clk_fout_kpll.rate = s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS5_KPLL_CON0));
}
#else
#define exynos5420_clock_suspend NULL
#define exynos5420_clock_resume NULL
#endif

struct syscore_ops exynos5420_clock_syscore_ops = {
	.suspend	= exynos5420_clock_suspend,
	.resume		= exynos5420_clock_resume,
};

void __init_or_cpufreq exynos5420_setup_clocks(void)
{
	struct clk *xtal_clk;

	unsigned long xtal;
	unsigned long apll;
	unsigned long kpll;
	unsigned long mpll;
	unsigned long bpll;
	unsigned long cpll;
	unsigned long vpll;
	unsigned long ipll;
	unsigned long dpll;
	unsigned long epll;
	unsigned long rpll;
	unsigned long spll;

	unsigned int i;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	xtal_clk = clk_get(NULL, "xtal");
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);

	xtal_rate = xtal;

	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	clk_fout_apll.ops = &exynos5420_fout_simple_ops;
	clk_fout_kpll.ops = &exynos5420_fout_simple_ops;
	clk_fout_cpll.ops = &exynos5420_cpll_ops;
	clk_fout_epll.ops = &exynos5420_epll_ops;
	clk_fout_vpll.ops = &exynos5420_vpll_ops;
	clk_fout_spll.ops = &exynos5420_spll_ops;
	clk_fout_mpll.ops = &exynos5420_mpll_ops;
	clk_fout_rpll.ops = &exynos5420_rpll_ops;
	clk_fout_bpll.ops = &exynos5420_bpll_ops;
	clk_ipll.ops = &exynos5420_ipll_ops;
	clk_fout_dpll.ops = &exynos5420_dpll_ops;

	exynos5420_pll_init();

	/* Set and check PLLs */
	apll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_APLL_CON0));
	kpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_KPLL_CON0));
	mpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_MPLL_CON0));
	bpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_BPLL_CON0));
	cpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_CPLL_CON0));
	vpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_VPLL_CON0));
	ipll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_IPLL_CON0));
	dpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_DPLL_CON0));
	epll = s5p_get_pll36xx(xtal, __raw_readl(EXYNOS5_EPLL_CON0),
		__raw_readl(EXYNOS5_EPLL_CON1));
	rpll = s5p_get_pll36xx(xtal, __raw_readl(EXYNOS5_RPLL_CON0),
		__raw_readl(EXYNOS5_RPLL_CON1));
	spll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_SPLL_CON0));

	clk_fout_apll.rate = apll;
	clk_fout_bpll.rate = bpll;
	clk_fout_cpll.rate = cpll;
	clk_fout_mpll.rate = mpll;
	clk_fout_epll.rate = epll;
	clk_fout_vpll.rate = vpll;
	clk_fout_kpll.rate = kpll;
	clk_fout_dpll.rate = dpll;
	clk_fout_rpll.rate = rpll;
	clk_fout_spll.rate = spll;
	clk_ipll.rate = ipll;

	for (i = 0; i < ARRAY_SIZE(exynos5420_sysclks); i++)
		s3c_set_clksrc(exynos5420_sysclks[i], true);

	for (i = 0; i < ARRAY_SIZE(exynos5420_clksrcs); i++)
		s3c_set_clksrc(&exynos5420_clksrcs[i], true);

	clk_set_parent(&exynos5420_clk_mout_ipll.clk, &exynos5420_clk_fout_ipll.clk);
	clk_set_parent(&exynos5420_clk_fout_ipll.clk, &clk_fin_ipll);

#ifndef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
	clk_enable(&clk_fout_spll);
	clk_set_parent(&exynos5420_clk_fout_ipll.clk, &clk_ipll);
#endif
}

static struct clk *exynos5_clks_off[] __initdata = {
	&clk_fout_epll,
};

void __init exynos5420_register_clocks(void)
{
	int ptr;
	struct clksrc_clk *clksrc;

	clk_fout_apll.enable = exynos5_apll_ctrl;
	clk_fout_apll.ctrlbit = (1 << 22);
	clk_fout_epll.enable = exynos5_epll_ctrl;
	clk_fout_epll.ctrlbit = (1 << 4);
	clk_fout_spll.enable = exynos5_spll_enable;
	clk_fout_spll.ctrlbit = (1 << 22);
	clk_fout_vpll.enable = exynos5_vpll_enable;
	clk_fout_vpll.ctrlbit = (1 << 22);
	/* RPLL diabled */
	clk_fout_rpll.enable = exynos5_rpll_enable;
	clk_fout_rpll.ctrlbit = (1 << 31);

	clk_v_epll.enable = exynos5_v_epll_ctrl;
	clk_v_epll.ctrlbit = (1 << 31);
	clk_v_vpll.enable = exynos5_v_vpll_ctrl;
	clk_v_vpll.ctrlbit = (1 << 31);
	clk_v_ipll.enable = exynos5_v_ipll_ctrl;
	clk_v_ipll.ctrlbit = (1 << 31);

	s3c24xx_register_clocks(exynos5_clks_off, ARRAY_SIZE(exynos5_clks_off));
	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_clks_off); ptr++)
		s3c_disable_clocks(exynos5_clks_off[ptr], 1);

	s3c24xx_register_clocks(exynos5420_clks, ARRAY_SIZE(exynos5420_clks));

	s3c_disable_clocks(&clk_fout_vpll, 1);
#ifndef CONFIG_S5P_DP
	s3c_disable_clocks(&clk_fout_spll, 1);
#endif

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5420_sysclks); ptr++)
		s3c_register_clksrc(exynos5420_sysclks[ptr], 1);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5420_sysclks_off); ptr++) {
		clksrc = exynos5420_sysclks_off[ptr];
		s3c_register_clksrc(clksrc, 1);
		s3c_disable_clocks(&clksrc->clk, 1);
	}

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5420_clksrcs_off); ptr++) {
		clksrc = &exynos5420_clksrcs_off[ptr];
		s3c_register_clksrc(clksrc, 1);
		s3c_disable_clocks(&clksrc->clk, 1);
	}

	s3c_register_clksrc(exynos5420_clksrcs, ARRAY_SIZE(exynos5420_clksrcs));
	s3c_register_clocks(exynos5420_init_clocks, ARRAY_SIZE(exynos5420_init_clocks));

	s3c_register_clocks(exynos5420_init_clocks_off, ARRAY_SIZE(exynos5420_init_clocks_off));
	s3c_disable_clocks(exynos5420_init_clocks_off, ARRAY_SIZE(exynos5420_init_clocks_off));

	s3c_register_clocks(exynos5_i2cs_clocks, ARRAY_SIZE(exynos5_i2cs_clocks));
	s3c_disable_clocks(exynos5_i2cs_clocks, ARRAY_SIZE(exynos5_i2cs_clocks));

	s3c24xx_register_clocks(exynos5_clk_cdev, ARRAY_SIZE(exynos5_clk_cdev));
	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_clk_cdev); ptr++)
		s3c_disable_clocks(exynos5_clk_cdev[ptr], 1);

	clkdev_add_table(exynos5420_clk_lookup, ARRAY_SIZE(exynos5420_clk_lookup));

	register_syscore_ops(&exynos5420_clock_syscore_ops);
#ifdef CONFIG_SAMSUNG_DEV_PWM
	s3c_pwmclk_init();
#endif
}
