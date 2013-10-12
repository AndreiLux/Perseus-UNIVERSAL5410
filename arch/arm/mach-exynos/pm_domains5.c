/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/suspend.h>

#include <mach/pm_domains.h>
#include <mach/pm_interrupt_domains.h>
#include <mach/bts.h>
#include <mach/regs-clock.h>
#include <mach/regs-mem.h>

#include <plat/clock-clksrc.h>

extern void exynos5_pm_powerdomain_init(struct exynos_pm_domain *domain);

/* exynos54xx power domain */
EXYNOS_COMMON_GPD(exynos54xx_pd_mfc, EXYNOS5410_MFC_CONFIGURATION, "pd-mfc");
EXYNOS_COMMON_GPD(exynos54xx_pd_maudio, EXYNOS5410_MAU_CONFIGURATION, "pd-maudio");
EXYNOS_COMMON_GPD(exynos54xx_pd_g3d, EXYNOS5410_G3D_CONFIGURATION, "pd-g3d");
EXYNOS_COMMON_GPD(exynos54xx_pd_disp1, EXYNOS5410_DISP1_CONFIGURATION, "pd-disp1");
EXYNOS_COMMON_GPD(exynos54xx_pd_gscl, EXYNOS5_GSCL_CONFIGURATION, "pd-gscl");
EXYNOS_COMMON_GPD(exynos54xx_pd_isp, EXYNOS5_ISP_CONFIGURATION, "pd-isp");
EXYNOS_COMMON_GPD(exynos54xx_spd_mipi_csis0, NULL, "pd-mipi-csis0");
EXYNOS_COMMON_GPD(exynos54xx_spd_mipi_csis1, NULL, "pd-mipi-csis1");
EXYNOS_COMMON_GPD(exynos54xx_spd_mipi_csis2, NULL, "pd-mipi-csis2");
EXYNOS_COMMON_GPD(exynos54xx_spd_flite0, NULL, "pd-flite0");
EXYNOS_COMMON_GPD(exynos54xx_spd_flite1, NULL, "pd-flite1");
EXYNOS_COMMON_GPD(exynos54xx_spd_flite2, NULL, "pd-flite2");

static void __iomem *exynos54xx_pwr_reg_g3d[] = {
	EXYNOS5_CMU_CLKSTOP_G3D_SYS_PWR_REG,
	EXYNOS5_CMU_SYSCLK_G3D_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_G3D_SYS_PWR_REG,
};

static void __iomem *exynos54xx_pwr_reg_mfc[] = {
	EXYNOS5_CMU_CLKSTOP_MFC_SYS_PWR_REG,
	EXYNOS5_CMU_SYSCLK_MFC_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_MFC_SYS_PWR_REG,
};

#ifdef CONFIG_SND_SAMSUNG_I2S
static void __iomem *exynos54xx_pwr_reg_mau[] = {
	EXYNOS5_CMU_CLKSTOP_MAU_SYS_PWR_REG,
	EXYNOS5_CMU_SYSCLK_MAU_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_MAU_SYS_PWR_REG,
};
#endif

static void __iomem *exynos54xx_pwr_reg_disp1[] = {
	EXYNOS5_CMU_CLKSTOP_DISP1_SYS_PWR_REG,
	EXYNOS5_CMU_SYSCLK_DISP1_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_DISP1_SYS_PWR_REG,
};

static void __iomem *exynos54xx_pwr_reg_gscl[] = {
	EXYNOS5_CMU_CLKSTOP_GSCL_SYS_PWR_REG,
	EXYNOS5_CMU_SYSCLK_GSCL_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_GSCL_SYS_PWR_REG,
};

#ifdef CONFIG_EXYNOS5_DEV_FIMC_IS
static void __iomem *exynos54xx_pwr_reg_isp[] = {
	EXYNOS5_CMU_CLKSTOP_ISP_SYS_PWR_REG,
	EXYNOS5_CMU_SYSCLK_ISP_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_ISP_SYS_PWR_REG,
};
#endif

static void __iomem *exynos5420_pwr_reg_mau[] = {
	EXYNOS5420_CMU_CLKSTOP_MAU_SYS_PWR_REG,
	EXYNOS5420_CMU_RESET_MAU_SYS_PWR_REG,
};

static struct clk *gscl_aclk_333_432_gscl_sub;
static struct clk *gscl_dout_aclk_333_432_gscl;
static struct clk *gscl_ext_xtal;
static struct clk *hdmi;

void __iomem *regs;


/* exynos5410 power domain */
EXYNOS_COMMON_GPD(exynos5410_pd_disp0, EXYNOS5410_DISP0_CONFIGURATION, "pd-disp0");
/* domains for disp1 */
EXYNOS_COMMON_GPD(exynos5410_spd_fimd1, NULL, "pd-fimd1");
EXYNOS_COMMON_GPD(exynos5410_spd_hdmi, NULL, "pd-hdmi");
EXYNOS_COMMON_GPD(exynos5410_spd_mixer, NULL, "pd-mixer");
EXYNOS_COMMON_GPD(exynos5410_spd_dp, NULL, "pd-dp");
EXYNOS_COMMON_GPD(exynos5410_spd_mipi_dsim1, NULL, "pd-mipi-dsim1");
/* domains for gscl */
EXYNOS_COMMON_GPD(exynos5410_spd_gscl0, NULL, "pd-gscl0");
EXYNOS_COMMON_GPD(exynos5410_spd_gscl1, NULL, "pd-gscl1");
EXYNOS_COMMON_GPD(exynos5410_spd_gscl2, NULL, "pd-gscl2");
EXYNOS_COMMON_GPD(exynos5410_spd_gscl3, NULL, "pd-gscl3");
EXYNOS_COMMON_GPD(exynos5410_spd_gscl4, NULL, "pd-gscl4");
/* domains for g2d with bts */
EXYNOS_COMMON_GPD(exynos5410_spd_g2d, NULL, "pd-g2d");

/* exynos5 code */
static int exynos5_pm_domain_bts_on(struct exynos_pm_domain *domain)
{
	bts_initialize(domain->pd.name, true);

	return 0;
}

static int exynos5_pm_domain_bts_off(struct exynos_pm_domain *domain)
{
	bts_initialize(domain->pd.name, false);

	return 0;
}

static int exynos5_pm_domain_maudio_pre_power_off(struct exynos_pm_domain *domain)
{
	__raw_writel(0, EXYNOS5_PAD_RETENTION_MAU_SYS_PWR_REG);

	return 0;
}

static int exynos5_pm_domain_maudio_post_power_on(struct exynos_pm_domain *domain)
{
	__raw_writel(0x10000000, EXYNOS_PAD_RET_MAUDIO_OPTION);

	return 0;
}

static int exynos5_pm_domain_disp1_post_power_control(struct exynos_pm_domain *domain)
{
	clk_enable(hdmi);
	__raw_writel(0x1, regs  + 0x30);
	pr_info("HDMI phy power off : %x\n", __raw_readl(regs + 0x30));
	clk_disable(hdmi);

	return 0;
}

/* exynos5410 code */
static int exynos5410_pd_gscl_clock_control(bool on)
{
	struct clk *target_parent;
	const char *msg_status;

	if (IS_ERR(gscl_aclk_333_432_gscl_sub)) {
		pr_err("PM DOMAIN : clk_get(aclk_333_432_gscl_sub) failed\n");
		return PTR_ERR(gscl_aclk_333_432_gscl_sub);
	}

	if (on) {
		/*
		 * if GSCL power domain is turned on
		 * parent of aclk_333_432_gscl is set with
		 * dout_aclk_333_432_gscl
		 */
		target_parent = gscl_dout_aclk_333_432_gscl;
		msg_status = "on";
	} else {
		/*
		 * if GSCL power domain is turned off
		 * parent of aclk_333_432_gscl is set with ext_xtal
		 */
		target_parent = gscl_ext_xtal;
		msg_status = "off";
	}

	if (IS_ERR(target_parent)) {
		pr_err("PM DOMAIN : clk_get(target_parent) failed[turn %s]\n", msg_status);
		return PTR_ERR(target_parent);
	}

	clk_set_parent(gscl_aclk_333_432_gscl_sub, target_parent);

	return 0;
}

static void exynos5410_gscl_ctrl_save(bool on)
{
	static unsigned int gscl0, gscl1;

	if (on) {
		gscl0 = __raw_readl(EXYNOS5_CLKGATE_IP_GSCL0);
		__raw_writel(0xFFFFFFFF, EXYNOS5_CLKGATE_IP_GSCL0);
		gscl1 = __raw_readl(EXYNOS5_CLKGATE_IP_GSCL);
		__raw_writel(0x0000FFFF, EXYNOS5_CLKGATE_IP_GSCL);
	} else {
		__raw_writel(gscl0, EXYNOS5_CLKGATE_IP_GSCL0);
		__raw_writel(gscl1, EXYNOS5_CLKGATE_IP_GSCL);
	}
}

static int exynos5410_pm_domain_gscl_pre_power_control(struct exynos_pm_domain *domain)
{
	exynos5410_gscl_ctrl_save(true);

	return 0;
}

static int exynos5410_pm_domain_gscl_pre_power_off(struct exynos_pm_domain *domain)
{
	exynos5410_pd_gscl_clock_control(false);

	return 0;
}

static int exynos5410_pm_domain_gscl_post_power_on(struct exynos_pm_domain *domain)
{
	exynos5410_pd_gscl_clock_control(true);

	return 0;
}

static int exynos5410_pm_domain_gscl_post_power_control(struct exynos_pm_domain *domain)
{
	exynos5410_gscl_ctrl_save(false);

	return 0;
}

#ifdef CONFIG_EXYNOS5_DEV_FIMC_IS
static int exynos5410_pm_domain_isp_power_control(struct exynos_pm_domain *domain, int power_flag)
{
	int ret = exynos_pm_domain_power_control(domain, power_flag);
	int tmp, timeout = 500;

	if (ret) {
		__raw_writel(0, EXYNOS5410_ISP_ARM_OPTION);

		do {
			tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
			usleep_range(80, 100);
			timeout--;
		} while ((tmp != power_flag) && timeout);

		if (!timeout) {
			pr_err("PM DOMAIN : ISP WFI unset power down fail(state:%x)\n",
				__raw_readl(domain->base + 0x4));

			tmp = __raw_readl(EXYNOS5410_LPI_BUS_MASK0);
			tmp |= (EXYNOS5410_LPI_BUS_MASK0_ISP0 | EXYNOS5410_LPI_BUS_MASK0_ISP1);
			__raw_writel(tmp, EXYNOS5410_LPI_BUS_MASK0);

			tmp = __raw_readl(EXYNOS5410_LPI_BUS_MASK1);
			tmp |= EXYNOS5410_LPI_BUS_MASK1_P_ISP;
			__raw_writel(tmp, EXYNOS5410_LPI_BUS_MASK1);

			timeout = 100;

			do {
				tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
				udelay(1);
				timeout--;
			} while ((tmp != power_flag) && timeout);

			if (!timeout) {
				pr_err("CG_STATUS0 : %08X\n", __raw_readl(EXYNOS5410_CG_STATUS0));

				tmp = __raw_readl(EXYNOS5410_LPI_MASK0);
				tmp |= EXYNOS5410_LPI_MASK0_FD;
				__raw_writel(tmp, EXYNOS5410_LPI_MASK0);
				pr_err("FD Disable(LPI : %08X)\n", __raw_readl(EXYNOS5410_LPI_MASK0));

				timeout = 100;
				do {
					tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
					udelay(1);
					timeout--;
				} while ((tmp != power_flag) && timeout);

				if (!timeout) {
					pr_err("CG_STATUS0 : %08X\n", __raw_readl(EXYNOS5410_CG_STATUS0));

					tmp = __raw_readl(EXYNOS5410_LPI_MASK0);
					tmp |= EXYNOS5410_LPI_MASK0_OTHERS;
					__raw_writel(tmp, EXYNOS5410_LPI_MASK0);
					pr_err("Others Disable(LPI : %08X)\n", __raw_readl(EXYNOS5410_LPI_MASK0));

					timeout = 100;
					do {
						tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
						udelay(1);
						timeout--;
					} while ((tmp != power_flag) && timeout);

					if (!timeout)
						pr_err("ISP force timeout fail\n");
				}
			} else {
				pr_err("PM DOMAIN : ISP force timeout success\n");
				tmp = __raw_readl(EXYNOS5410_LPI_BUS_MASK0);
				tmp &= ~(EXYNOS5410_LPI_BUS_MASK0_ISP0 | EXYNOS5410_LPI_BUS_MASK0_ISP1);

				__raw_writel(tmp, EXYNOS5410_LPI_BUS_MASK0);

				tmp = __raw_readl(EXYNOS5410_LPI_BUS_MASK1);
				tmp &= ~EXYNOS5410_LPI_BUS_MASK1_P_ISP;
				__raw_writel(tmp, EXYNOS5410_LPI_BUS_MASK1);
			}
		} else {
			pr_err("PM DOMAIN : ISP WFI unset power down success\n");
		}
	}

	return 0;
}

static int exynos5420_pm_domain_isp_power_control(struct exynos_pm_domain *domain, int power_flag)
{
	int ret = exynos_pm_domain_power_control(domain, power_flag);
	int tmp, timeout = 500;

	if (ret) {
		__raw_writel(0, EXYNOS5420_ISP_ARM_OPTION);

		do {
			tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
			usleep_range(80, 100);
			timeout--;
		} while ((tmp != power_flag) && timeout);

		if (!timeout) {
			pr_err("PM DOMAIN : ISP WFI unset power down fail(state:%x)\n",
				__raw_readl(domain->base + 0x4));

			tmp = __raw_readl(EXYNOS5420_LPI_BUS_MASK0);
			tmp |= (EXYNOS5420_LPI_BUS_MASK0_ISP0 |
				EXYNOS5420_LPI_BUS_MASK0_ISP1 |
				EXYNOS5420_LPI_BUS_MASK0_RSTOP_ISP |
				EXYNOS5420_LPI_BUS_MASK0_PRSTOP_ISP);
			__raw_writel(tmp, EXYNOS5420_LPI_BUS_MASK0);

			timeout = 100;

			do {
				tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
				udelay(1);
				timeout--;
			} while ((tmp != power_flag) && timeout);

			if (!timeout) {
				pr_err("CG_STATUS0 : %08X\n", __raw_readl(EXYNOS5420_CG_STATUS0));

				tmp = __raw_readl(EXYNOS5420_LPI_MASK0);
				tmp |= EXYNOS5420_LPI_MASK0_FD;
				__raw_writel(tmp, EXYNOS5420_LPI_MASK0);
				pr_err("FD Disable(LPI : %08X)\n", __raw_readl(EXYNOS5420_LPI_MASK0));

				timeout = 100;
				do {
					tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
					udelay(1);
					timeout--;
				} while ((tmp != power_flag) && timeout);

				if (!timeout) {
					pr_err("CG_STATUS0 : %08X\n", __raw_readl(EXYNOS5420_CG_STATUS0));

					tmp = __raw_readl(EXYNOS5420_LPI_MASK0);
					tmp |= EXYNOS5420_LPI_MASK0_OTHERS;
					__raw_writel(tmp, EXYNOS5420_LPI_MASK0);
					pr_err("Others Disable(LPI : %08X)\n", __raw_readl(EXYNOS5420_LPI_MASK0));

					timeout = 100;
					do {
						tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
						udelay(1);
						timeout--;
					} while ((tmp != power_flag) && timeout);

					if (!timeout)
						pr_err("ISP force timeout fail\n");
				}
			} else {
				pr_err("PM DOMAIN : ISP force timeout success\n");
				tmp = __raw_readl(EXYNOS5420_LPI_BUS_MASK0);
				tmp &= ~(EXYNOS5420_LPI_BUS_MASK0_ISP0 |
					EXYNOS5420_LPI_BUS_MASK0_ISP1 |
					EXYNOS5420_LPI_BUS_MASK0_RSTOP_ISP |
					EXYNOS5420_LPI_BUS_MASK0_PRSTOP_ISP);
				__raw_writel(tmp, EXYNOS5420_LPI_BUS_MASK0);
			}
		} else {
			pr_err("PM DOMAIN : ISP WFI unset power down success\n");
		}
	}

	return 0;
}
#endif

static int exynos5410_pm_domain_notifier(struct notifier_block *notifier,
				unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pm_genpd_poweron(&exynos54xx_pd_g3d.pd);
		break;
	case PM_POST_SUSPEND:
		bts_drex_initialize();
		bts_initialize("pd-eagle", true);
		bts_initialize("pd_kfc", true);
		break;
	}

	return NOTIFY_DONE;
}

static int exynos5410_pm_domain_init(void)
{
	int i;

	gscl_aclk_333_432_gscl_sub = clk_get(NULL, "aclk_333_432_gscl");
	gscl_dout_aclk_333_432_gscl = clk_get(NULL, "dout_aclk_333_432_gscl");
	gscl_ext_xtal = clk_get(NULL, "ext_xtal");
	hdmi = clk_get(&s5p_device_hdmi.dev, "hdmi");

	exynos_pm_powerdomain_init(&exynos54xx_pd_isp);
	exynos_pm_powerdomain_init(&exynos5410_pd_disp0);
	exynos_pm_powerdomain_init(&exynos5410_spd_g2d);
	exynos_pm_powerdomain_init(&exynos5410_spd_fimd1);
	exynos_pm_powerdomain_init(&exynos5410_spd_mipi_dsim1);
	exynos_pm_powerdomain_init(&exynos5410_spd_dp);
	exynos_pm_powerdomain_init(&exynos5410_spd_hdmi);
	exynos_pm_powerdomain_init(&exynos5410_spd_mixer);
	exynos_pm_powerdomain_init(&exynos5410_spd_gscl0);
	exynos_pm_powerdomain_init(&exynos5410_spd_gscl1);
	exynos_pm_powerdomain_init(&exynos5410_spd_gscl2);
	exynos_pm_powerdomain_init(&exynos5410_spd_gscl3);
	exynos_pm_powerdomain_init(&exynos5410_spd_gscl4);
	exynos_pm_powerdomain_init(&exynos54xx_spd_mipi_csis0);
	exynos_pm_powerdomain_init(&exynos54xx_spd_mipi_csis1);
	exynos_pm_powerdomain_init(&exynos54xx_spd_mipi_csis2);
	exynos_pm_powerdomain_init(&exynos54xx_spd_flite0);
	exynos_pm_powerdomain_init(&exynos54xx_spd_flite1);
	exynos_pm_powerdomain_init(&exynos54xx_spd_flite2);

#ifdef CONFIG_S5P_DEV_MFC
	exynos_pm_add_platdev(&exynos54xx_pd_mfc, &s5p_device_mfc);
	exynos_pm_add_clk(&exynos54xx_pd_mfc, &s5p_device_mfc.dev, "mfc");
	exynos_pm_add_clk(&exynos54xx_pd_mfc, NULL, "mfc.ppmu");
	exynos_pm_add_clk(&exynos54xx_pd_mfc, &SYSMMU_PLATDEV(mfc_lr).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_callback(&exynos54xx_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_mfc, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);

	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_mfc); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				exynos54xx_pwr_reg_mfc[i], 0);
#endif
	exynos_pm_add_platdev(&exynos54xx_pd_g3d, &exynos5_device_g3d);
	exynos_pm_add_clk(&exynos54xx_pd_g3d, NULL, "sgx_core");
	exynos_pm_add_clk(&exynos54xx_pd_g3d, NULL, "sgx_hyd");
	exynos_pm_add_callback(&exynos54xx_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_g3d, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);

	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_g3d); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				exynos54xx_pwr_reg_g3d[i], 0);
#ifdef CONFIG_SND_SAMSUNG_I2S
	exynos_pm_add_platdev(&exynos54xx_pd_maudio, &exynos5_device_i2s0);
	exynos_pm_add_clk(&exynos54xx_pd_maudio, &exynos5_device_i2s1.dev, "iis");
	exynos_pm_add_clk(&exynos54xx_pd_maudio, &exynos5_device_i2s2.dev, "iis");

	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_maudio_pre_power_off);
	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_maudio_post_power_on);

	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_mau); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				exynos54xx_pwr_reg_mau[i], 0);
#endif
#ifdef CONFIG_SND_SAMSUNG_ALP
	exynos_pm_add_platdev(&exynos54xx_pd_maudio, &exynos5_device_srp);
#endif
#ifdef CONFIG_S5P_DEV_FIMD1
	exynos_pm_add_platdev(&exynos5410_spd_fimd1, &s5p_device_fimd1);
	exynos_pm_add_clk(&exynos5410_spd_fimd1, &s5p_device_fimd1.dev, "fimd");
	exynos_pm_add_clk(&exynos5410_spd_fimd1, NULL, "axi_disp1");
	exynos_pm_add_clk(&exynos5410_spd_fimd1, NULL, "mie1");
	exynos_pm_add_clk(&exynos5410_spd_fimd1, NULL, "dp1");
	exynos_pm_add_clk(&exynos5410_spd_fimd1, &SYSMMU_PLATDEV(fimd1).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5410_spd_fimd1, true);
	exynos_pm_add_callback(&exynos54xx_pd_disp1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_disp1, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);

	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_disp1); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_disp1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				exynos54xx_pwr_reg_disp1[i], 0);
#endif
#ifdef CONFIG_S5P_DEV_MIPI_DSIM1
	exynos_pm_add_platdev(&exynos5410_spd_mipi_dsim1, &s5p_device_mipi_dsim1);
	exynos_pm_add_clk(&exynos5410_spd_mipi_dsim1, &s5p_device_mipi_dsim1.dev, "dsim1");
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5410_spd_mipi_dsim1, true);
#endif
#ifdef CONFIG_S5P_DEV_DP
	exynos_pm_add_platdev(&exynos5410_spd_dp, &s5p_device_dp);
	exynos_pm_add_clk(&exynos5410_spd_dp, &s5p_device_dp.dev, "dp");
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5410_spd_dp, true);
#endif
#ifdef CONFIG_S5P_DEV_TV
	exynos_pm_add_platdev(&exynos5410_spd_hdmi, &s5p_device_hdmi);
	exynos_pm_add_clk(&exynos5410_spd_hdmi, &s5p_device_hdmi.dev, "hdmi");
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5410_spd_hdmi, true);

	exynos_pm_add_platdev(&exynos5410_spd_mixer, &s5p_device_mixer);
	exynos_pm_add_clk(&exynos5410_spd_mixer, &s5p_device_mixer.dev, "mixer");
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5410_spd_mixer, true);

	exynos_pm_add_clk(&exynos5410_spd_mixer, &SYSMMU_PLATDEV(tv).dev, SYSMMU_CLOCK_NAME);
#endif
	exynos_pm_add_callback(&exynos54xx_pd_disp1, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				true, exynos5_pm_domain_disp1_post_power_control);
#ifdef CONFIG_EXYNOS5_DEV_FIMC_IS
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &exynos5_device_fimc_is);
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &s3c64xx_device_spi3);
	exynos_pm_add_clk(&exynos54xx_pd_isp, &exynos5_device_fimc_is.dev, NULL);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_pd_isp, false);
	exynos_pm_add_callback(&exynos54xx_pd_isp, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_isp, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	if (soc_is_exynos5420()) {
		exynos54xx_pd_isp.on	= exynos5420_pm_domain_isp_power_control;
		exynos54xx_pd_isp.off	= exynos5420_pm_domain_isp_power_control;
	} else {
		exynos54xx_pd_isp.on	= exynos5410_pm_domain_isp_power_control;
		exynos54xx_pd_isp.off	= exynos5410_pm_domain_isp_power_control;
	}

	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_isp); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_isp, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				exynos54xx_pwr_reg_isp[i], 0);
#endif
#ifdef CONFIG_S5P_DEV_FIMG2D
	/* g2d power domain is logical power domain for BTS */
	exynos_pm_add_platdev(&exynos5410_spd_g2d, &s5p_device_fimg2d);
	exynos_pm_add_clk(&exynos5410_spd_g2d, &s5p_device_fimg2d.dev, "fimg2d");
	exynos_pm_add_callback(&exynos5410_spd_g2d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos5410_spd_g2d, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
#endif
#ifdef CONFIG_EXYNOS5_DEV_GSC
	exynos_pm_add_platdev(&exynos5410_spd_gscl0, &exynos5_device_gsc0);
	exynos_pm_add_clk(&exynos5410_spd_gscl0, &exynos5_device_gsc0.dev, "gscl");
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5410_spd_gscl0, true);

	exynos_pm_add_platdev(&exynos5410_spd_gscl1, &exynos5_device_gsc1);
	exynos_pm_add_clk(&exynos5410_spd_gscl1, &exynos5_device_gsc1.dev, "gscl");
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5410_spd_gscl1, true);

	exynos_pm_add_platdev(&exynos5410_spd_gscl2, &exynos5_device_gsc2);
	exynos_pm_add_clk(&exynos5410_spd_gscl2, &exynos5_device_gsc2.dev, "gscl");
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5410_spd_gscl2, true);

	exynos_pm_add_platdev(&exynos5410_spd_gscl3, &exynos5_device_gsc3);
	exynos_pm_add_clk(&exynos5410_spd_gscl3, &exynos5_device_gsc3.dev, "gscl");
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5410_spd_gscl3, true);
#ifdef CONFIG_EXYNOS5_DEV_SCALER
	exynos_pm_add_platdev(&exynos5410_spd_gscl4, &exynos5_device_scaler0);
	exynos_pm_add_clk(&exynos5410_spd_gscl4, &exynos5_device_scaler0.dev, "sc-pclk");
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5410_spd_gscl4, true);
#endif
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				true, exynos5410_pm_domain_gscl_pre_power_control);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5410_pm_domain_gscl_pre_power_off);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ONOFF,
				false, exynos5410_pm_domain_gscl_post_power_control);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5410_pm_domain_gscl_post_power_on);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
	exynos_pm_add_platdev(&exynos54xx_spd_mipi_csis0, &s5p_device_mipi_csis0);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_mipi_csis0, true);

	exynos_pm_add_platdev(&exynos54xx_spd_mipi_csis1, &s5p_device_mipi_csis1);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_mipi_csis1, true);

	exynos_pm_add_platdev(&exynos54xx_spd_mipi_csis2, &s5p_device_mipi_csis2);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_mipi_csis2, true);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	exynos_pm_add_platdev(&exynos54xx_spd_flite0, &exynos_device_flite0);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_flite0, true);

	exynos_pm_add_platdev(&exynos54xx_spd_flite1, &exynos_device_flite1);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_flite1, true);

	exynos_pm_add_platdev(&exynos54xx_spd_flite2, &exynos_device_flite2);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_flite2, true);
#endif
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif0));
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif1));
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif2));
	exynos_pm_add_platdev(&exynos5410_spd_gscl0, &SYSMMU_PLATDEV(gsc0));
	exynos_pm_add_platdev(&exynos5410_spd_gscl1, &SYSMMU_PLATDEV(gsc1));
	exynos_pm_add_platdev(&exynos5410_spd_gscl2, &SYSMMU_PLATDEV(gsc2));
	exynos_pm_add_platdev(&exynos5410_spd_gscl3, &SYSMMU_PLATDEV(gsc3));
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(isp3));
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(s3d));
#ifdef CONFIG_EXYNOS5_DEV_SCALER
	exynos_pm_add_platdev(&exynos5410_spd_gscl4, &SYSMMU_PLATDEV(scaler));
#endif
	exynos_pm_add_platdev(&exynos5410_spd_fimd1, &SYSMMU_PLATDEV(fimd1));
	exynos_pm_add_platdev(&exynos5410_spd_mixer, &SYSMMU_PLATDEV(tv));
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &SYSMMU_PLATDEV(isp0));
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &SYSMMU_PLATDEV(isp1));
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &SYSMMU_PLATDEV(isp2));
	exynos_pm_add_platdev(&exynos54xx_pd_mfc, &SYSMMU_PLATDEV(mfc_lr));
	exynos_pm_add_platdev(&exynos5410_spd_g2d, &SYSMMU_PLATDEV(2d));

	/* Initialize BTS */
	bts_initialize("pd-mfc", true);
	bts_initialize("pd-gscl", true);
	bts_initialize("pd-g3d", true);

	pm_notifier(exynos5410_pm_domain_notifier, 0);

	return 0;
}

/* exynos5420 power domain */
/* domains for disp1 */
EXYNOS_COMMON_GPD(exynos5420_spd_fimd1, NULL, "pd-fimd1");
EXYNOS_COMMON_GPD(exynos5420_spd_mixer, NULL, "pd-mixer");
EXYNOS_COMMON_GPD(exynos5420_spd_hdmi, NULL, "pd-hdmi");
EXYNOS_COMMON_GPD(exynos5420_spd_dp, NULL, "pd-dp");
EXYNOS_COMMON_GPD(exynos5420_spd_dsim1, NULL, "pd-dsim1");
/* domains for gscl */
EXYNOS_COMMON_GPD(exynos5420_spd_gscaler0, NULL, "pd-gscaler0");
EXYNOS_COMMON_GPD(exynos5420_spd_gscaler1, NULL, "pd-gscaler1");
EXYNOS_COMMON_GPD(exynos5420_spd_fimclite, NULL, "pd-fimclite");
EXYNOS_COMMON_GPD(exynos5420_spd_csis, NULL, "pd-csis");
/* domains for mscl */
EXYNOS_COMMON_GPD(exynos5420_pd_mscl, EXYNOS5420_MSC_CONFIGURATION, "pd-mscl");
EXYNOS_COMMON_GPD(exynos5420_spd_mscl0, NULL, "pd-mscl0");
EXYNOS_COMMON_GPD(exynos5420_spd_mscl1, NULL, "pd-mscl1");
EXYNOS_COMMON_GPD(exynos5420_spd_mscl2, NULL, "pd-mscl2");
/* domains for fsys */
EXYNOS_COMMON_GPD(exynos5420_pd_fsys, EXYNOS5420_FSYS_CONFIGURATION, "pd-fsys");
EXYNOS_COMMON_GPD(exynos5420_spd_usbdrd30, NULL, "pd-usbdrd30");
EXYNOS_COMMON_GPD(exynos5420_spd_pdma, NULL, "pd-pdma");
EXYNOS_COMMON_GPD(exynos5420_spd_usbhost20, NULL, "pd-usbhost20");
/* domains for fsys2 */
EXYNOS_COMMON_GPD(exynos5420_pd_fsys2, EXYNOS5420_FSYS2_CONFIGURATION, "pd-fsys2");
EXYNOS_COMMON_GPD(exynos5420_spd_mmc, NULL, "pd-mmc");
EXYNOS_COMMON_GPD(exynos5420_spd_sromc, NULL, "pd-sromc");
EXYNOS_COMMON_GPD(exynos5420_spd_ufs, NULL, "pd-ufs");
/* domains for psgen */
EXYNOS_COMMON_GPD(exynos5420_pd_psgen, EXYNOS5420_PSGEN_CONFIGURATION, "pd-psgen");
EXYNOS_COMMON_GPD(exynos5420_spd_psgengen, NULL, "pd-psgengen");
EXYNOS_COMMON_GPD(exynos5420_spd_rotator, NULL, "pd-rotator");
EXYNOS_COMMON_GPD(exynos5420_spd_mdma, NULL, "pd-mdma");
EXYNOS_COMMON_GPD(exynos5420_spd_jpeg, NULL, "pd-jpeg");
EXYNOS_COMMON_GPD(exynos5420_spd_psgenperis, NULL, "pd-psgenperis");
EXYNOS_COMMON_GPD(exynos5420_spd_sysreg, NULL, "pd-sysreg");
EXYNOS_COMMON_GPD(exynos5420_spd_tzpc, NULL, "pd-tzpc");
EXYNOS_COMMON_GPD(exynos5420_spd_tmu, NULL, "pd-tmu");
EXYNOS_COMMON_GPD(exynos5420_spd_seckey, NULL, "pd-seckey");
/* domains for peric */
EXYNOS_COMMON_GPD(exynos5420_pd_peric, EXYNOS5420_PERIC_CONFIGURATION, "pd-peric");

static void __iomem *exynos5420_pwr_reg_disp1[] = {
	EXYNOS5420_CMU_CLKSTOP_DISP1_SYS_PWR_REG,
	EXYNOS5420_CMU_SYSCLK_DISP1_SYS_PWR_REG,
	EXYNOS5420_CMU_RESET_DISP1_SYS_PWR_REG,
};

static void __iomem *exynos5420_pwr_reg_mscl[] = {
	EXYNOS5420_CMU_CLKSTOP_MSC_SYS_PWR_REG,
	EXYNOS5420_CMU_SYSCLK_MSC_SYS_PWR_REG,
	EXYNOS5420_CMU_RESET_MSC_SYS_PWR_REG,
};

#ifdef CONFIG_S5P_DEV_DP
static struct clk *clk_400_disp1;
static struct clk *clk_400_disp1_parent;
#endif
static struct clk *clk_300_disp1;
static struct clk *clk_300_disp1_parent;
static struct clk *clk_200_disp1;
static struct clk *clk_200_disp1_parent;
static struct clk *clk_fimd1;
static struct clk *clk_fimd1_parent;
static struct clk *clk_mdnie1;
static struct clk *clk_mdnie1_parent;
static struct clk *clk_dp1_ext_mst_vid;
static struct clk *clk_dp1_ext_mst_vid_parent;
static struct clk *clk_mipi1;
static struct clk *clk_mipi1_parent;
static struct clk *clk_mdnie_pwm1;
static struct clk *clk_mdnie_pwm1_parent;
static struct clk *clk_hdmi;
static struct clk *clk_hdmi_parent;

static struct clk *clk_mout_epll;
static struct clk *clk_epll_xtal;
static struct clk *clk_v_epll;
static struct clk *clk_v_vpll;
static struct clk *clk_mout_vpll;
static struct clk *fout_epll;
static struct clk *fout_vpll;
static struct clk *clk_vpll_xtal;
#if 0
static struct clk *clk_ipll_xtal;
static struct clk *clk_v_ipll;
static struct clk *fout_ipll;
static struct clk *clk_mout_ipll;
#endif

static DEFINE_SPINLOCK(clk_div2_ratio0_lock);

#define SHIFT_GSCL_BLK_300_DIV		4
#define SHIFT_GSCL_BLK_333_432_DIV	6
#define SHIFT_DISP1_BLK_DIV		16
#define SHIFT_MSCL_BLK_DIV		28
#define SHIFT_MFC_BLK_DIV		0

static void exynos5420_bts_initialize(void)
{

	exynos_pm_add_callback(&exynos54xx_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_g3d, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);

	exynos_pm_add_callback(&exynos5420_spd_fimd1, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos5420_spd_fimd1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_bts_off);

	exynos_pm_add_callback(&exynos5420_spd_mixer, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos5420_spd_mixer, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_bts_off);

	exynos_pm_add_callback(&exynos5420_spd_gscaler0, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos5420_spd_gscaler0, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_bts_off);

	exynos_pm_add_callback(&exynos5420_spd_gscaler1, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos5420_spd_gscaler1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_bts_off);

	exynos_pm_add_callback(&exynos5420_spd_fimclite, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos5420_spd_fimclite, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_bts_off);

	exynos_pm_add_callback(&exynos5420_pd_mscl,EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_callback(&exynos5420_pd_mscl,EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_bts_off);

	bts_initialize("pd-g3d", true);
	bts_initialize("pd-fimd1", true);
	bts_initialize("pd-mixer", true);
	bts_initialize("pd-mfc", true);
	bts_initialize("pd-gscaler0", true);
	bts_initialize("pd-gscaler1", true);
	bts_initialize("pd-mscl", true);
	bts_initialize("pd-g2d", true);
	bts_initialize("DEFAULT", true);
}

static void exynos5420_pm_domain_set_fake_rate(void __iomem *regs, unsigned int shift_val)
{
	unsigned int clk_div2_ratio0_value;
	unsigned int clk_div2_ratio0_old;

	clk_div2_ratio0_value = __raw_readl(regs);
	clk_div2_ratio0_old = (clk_div2_ratio0_value >> shift_val) & 0x3;
	clk_div2_ratio0_value &= ~(0x3 << shift_val);
	clk_div2_ratio0_value |= (((clk_div2_ratio0_old + 1) & 0x3) << shift_val);
	__raw_writel(clk_div2_ratio0_value, regs);

	clk_div2_ratio0_value = __raw_readl(regs);
	clk_div2_ratio0_value &= ~(0x3 << shift_val);
	clk_div2_ratio0_value |= (((clk_div2_ratio0_old) & 0x3) << shift_val);
	__raw_writel(clk_div2_ratio0_value, regs);
}

static int exynos5420_pm_domain_disp1_post_power_on(struct exynos_pm_domain *domain)
{
	spin_lock(&clk_div2_ratio0_lock);
	exynos5420_pm_domain_set_fake_rate(EXYNOS5_CLKDIV2_RATIO0, SHIFT_DISP1_BLK_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	clk_set_parent(clk_300_disp1, clk_300_disp1_parent);
	clk_set_parent(clk_200_disp1, clk_200_disp1_parent);
	clk_set_parent(clk_fimd1, clk_fimd1_parent);
	clk_set_parent(clk_mdnie1, clk_mdnie1_parent);
	clk_set_parent(clk_dp1_ext_mst_vid, clk_dp1_ext_mst_vid_parent);
	clk_set_parent(clk_mipi1, clk_mipi1_parent);
	clk_set_parent(clk_mdnie_pwm1, clk_mdnie_pwm1_parent);
	clk_set_parent(clk_hdmi, clk_hdmi_parent);
#ifdef CONFIG_S5P_DEV_DP
	if (soc_is_exynos5420())
		clk_set_parent(clk_400_disp1, clk_400_disp1_parent);
#endif

	return 0;
}

static int exynos5420_pm_domain_disp1_pre_power_off(struct exynos_pm_domain *domain)
{
#ifdef CONFIG_S5P_DEV_DP
	if (soc_is_exynos5420())
		clk_400_disp1_parent = clk_get_parent(clk_400_disp1);
#endif
	clk_300_disp1_parent = clk_get_parent(clk_300_disp1);
	clk_200_disp1_parent = clk_get_parent(clk_200_disp1);
	clk_fimd1_parent = clk_get_parent(clk_fimd1);
	clk_mdnie1_parent = clk_get_parent(clk_mdnie1);
	clk_dp1_ext_mst_vid_parent = clk_get_parent(clk_dp1_ext_mst_vid);
	clk_mipi1_parent = clk_get_parent(clk_mipi1);
	clk_mdnie_pwm1_parent = clk_get_parent(clk_mdnie_pwm1);
	clk_hdmi_parent = clk_get_parent(clk_hdmi);

	return 0;
}

static int exynos5420_pm_domain_mscl_post_power_on(struct exynos_pm_domain *domain)
{
	spin_lock(&clk_div2_ratio0_lock);
	exynos5420_pm_domain_set_fake_rate(EXYNOS5_CLKDIV2_RATIO0, SHIFT_MSCL_BLK_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	return 0;
}

static int exynos5420_pm_domain_gscl_post_power_on(struct exynos_pm_domain *domain)
{
	spin_lock(&clk_div2_ratio0_lock);
	exynos5420_pm_domain_set_fake_rate(EXYNOS5_CLKDIV2_RATIO0, SHIFT_GSCL_BLK_300_DIV);
	exynos5420_pm_domain_set_fake_rate(EXYNOS5_CLKDIV2_RATIO0, SHIFT_GSCL_BLK_333_432_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	return 0;
}

static int exynos5420_pm_domain_mfc_post_power_on(struct exynos_pm_domain *domain)
{
	/* Do not acquire lock to synchronize for MFC BLK */
	exynos5420_pm_domain_set_fake_rate(EXYNOS5_CLKDIV4_RATIO, SHIFT_MFC_BLK_DIV);

	return 0;
}

static int exynos5420_pm_domain_maudio_pre_power_control(struct exynos_pm_domain *domain)
{
	return 0;
}

static int exynos5420_pm_domain_maudio_pre_power_on(struct exynos_pm_domain *domain)
{
	/* Turn on EPLL before maudio block on */
	if (clk_enable(clk_v_epll))
		pr_err("%s: failed\n", __func__);
	udelay(100);
	return 0;
}

static int exynos5420_pm_domain_maudio_post_power_off(struct exynos_pm_domain *domain)
{
	clk_set_parent(clk_mout_epll, clk_epll_xtal);
	/* Turn off EPLL after maudio block off */
	clk_disable(clk_v_epll);
	return 0;
}

static int exynos5420_pm_domain_g3d_pre_power_on(struct exynos_pm_domain *domain)
{
	/* Turn on VPLL before maudio block on */
	if (clk_enable(clk_v_vpll))
		pr_err("%s: failed\n", __func__);
	udelay(70);

	clk_set_parent(clk_mout_vpll, fout_vpll);
	return 0;
}

static int exynos5420_pm_domain_g3d_post_power_off(struct exynos_pm_domain *domain)
{
	clk_set_parent(clk_mout_vpll, clk_vpll_xtal);
	/* Turn off VPLL after maudio block off */
	clk_disable(clk_v_vpll);
	return 0;
}

#if 0
/* Not implemented yet */
static int exynos5420_pm_domain_gscl_pre_power_on(struct exynos_pm_domain *domain)
{
	/* Turn on IPLL before ISP block on */
	if (clk_enable(clk_v_ipll))
		pr_err("%s: failed\n", __func__);
	udelay(60);

	clk_set_parent(clk_mout_ipll, fout_ipll);
	return 0;
}

static int exynos5420_pm_domain_gscl_post_power_off(struct exynos_pm_domain *domain)
{
	clk_set_parent(clk_mout_ipll, clk_ipll_xtal);
	/* Turn off IPLL after ISP block off */
	clk_disable(clk_v_ipll);
	return 0;
}
#endif

static int exynos5420_pm_domain_init(void)
{
	int i;

	spin_lock_init(&clk_div2_ratio0_lock);

	hdmi = clk_get(&s5p_device_hdmi.dev, "hdmi");

	clk_200_disp1 = clk_get(NULL, "aclk_200_disp1");
	clk_200_disp1_parent = clk_get(NULL, "aclk_200_sw");
	clk_300_disp1 = clk_get(NULL, "aclk_300_disp1");
	clk_300_disp1_parent = clk_get(NULL, "aclk_300_disp1_sw");
#ifdef CONFIG_S5P_DEV_DP
	if (soc_is_exynos5420()) {
		clk_400_disp1 = clk_get(NULL, "aclk_400_disp1");
		clk_400_disp1_parent = clk_get(NULL, "aclk_400_disp1_sw");
	}
#endif
	clk_fimd1 = clk_get(NULL, "mout_fimd1");
	clk_mdnie1 = clk_get(NULL, "mout_mdnie1");
	clk_dp1_ext_mst_vid = clk_get(NULL, "sclk_dp1_ext_mst_vid");
	clk_mipi1 = clk_get(NULL, "sclk_mipi1");
	clk_mdnie_pwm1 = clk_get(NULL, "sclk_mdnie_pwm1");
	clk_hdmi = clk_get(NULL, "sclk_hdmi");

	clk_mout_epll = clk_get(NULL, "mout_epll");
	clk_epll_xtal = clk_get(NULL, "ext_xtal");

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll))
		pr_err("Can't get clock[%s]\n", "fout_epll");

	clk_v_epll = clk_get(NULL, "v_epll");
	if (IS_ERR(clk_v_epll))
		pr_err("Can't get clock[%s]\n", "v_epll");
	clk_set_parent(clk_v_epll, clk_epll_xtal);
	clk_enable(clk_v_epll);

	clk_v_vpll = clk_get(NULL, "v_vpll");
	if (IS_ERR(clk_v_vpll))
		pr_err("Can't get clock[%s]\n", "v_vpll");

	clk_mout_vpll = clk_get(NULL, "mout_vpll");
	if (IS_ERR(clk_mout_vpll))
		pr_err("Can't get clock[%s]\n", "mout_vpll");

	fout_vpll = clk_get(NULL, "fout_vpll");
	if (IS_ERR(fout_vpll))
		pr_err("Can't get clock[%s]\n", "fout_vpll");

	clk_vpll_xtal = clk_epll_xtal;
	clk_set_parent(clk_v_epll, clk_vpll_xtal);
	clk_enable(clk_v_vpll);
#if 0
	/* Not implemented yet */
	clk_v_ipll = clk_get(NULL, "v_ipll");
	if (IS_ERR(clk_v_ipll))
		pr_err("Can't get clock[%s]\n", "v_ipll");

	clk_mout_ipll = clk_get(NULL, "mout_ipll");
	if (IS_ERR(clk_mout_ipll))
		pr_err("Can't get clock[%s]\n", "mout_ipll");

	fout_ipll = clk_get(NULL, "fout_ipll");
	if (IS_ERR(fout_ipll))
		pr_err("Can't get clock[%s]\n", "fout_ipll");

	clk_ipll_xtal = clk_epll_xtal;
	clk_set_parent(clk_v_epll, clk_ipll_xtal);
	clk_enable(clk_v_ipll);
#endif
	exynos_pm_powerdomain_init(&exynos5420_spd_fimd1);
	exynos_pm_powerdomain_init(&exynos5420_spd_mixer);
	exynos_pm_powerdomain_init(&exynos5420_spd_hdmi);
	exynos_pm_powerdomain_init(&exynos5420_spd_dp);
	exynos_pm_powerdomain_init(&exynos5420_spd_dsim1);
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5420_spd_hdmi, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5420_spd_fimd1, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5420_spd_dsim1, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5420_spd_mixer, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_disp1, &exynos5420_spd_dp, true);
	exynos_pm_powerdomain_init(&exynos5420_spd_gscaler0);
	exynos_pm_powerdomain_init(&exynos5420_spd_gscaler1);
	exynos_pm_powerdomain_init(&exynos5420_spd_fimclite);
	exynos_pm_powerdomain_init(&exynos5420_spd_csis);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5420_spd_gscaler0, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5420_spd_gscaler1, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5420_spd_fimclite, true);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos5420_spd_csis, true);
	exynos_pm_powerdomain_init(&exynos5420_pd_mscl);
	exynos_pm_powerdomain_init(&exynos5420_spd_mscl0);
	exynos_pm_powerdomain_init(&exynos5420_spd_mscl1);
	exynos_pm_powerdomain_init(&exynos5420_spd_mscl2);
	exynos_pm_add_subdomain(&exynos5420_pd_mscl, &exynos5420_spd_mscl0, true);
	exynos_pm_add_subdomain(&exynos5420_pd_mscl, &exynos5420_spd_mscl1, true);
	exynos_pm_add_subdomain(&exynos5420_pd_mscl, &exynos5420_spd_mscl2, true);

	exynos_pm_add_reg(&exynos54xx_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5410_G3D_OPTION, 2);
	exynos_pm_add_callback(&exynos54xx_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos5420_pm_domain_g3d_pre_power_on);
	exynos_pm_add_callback(&exynos54xx_pd_g3d, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_OFF,
				false, exynos5420_pm_domain_g3d_post_power_off);
	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_g3d); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
			exynos54xx_pwr_reg_g3d[i], 0);

	exynos_pm_add_reg(&exynos54xx_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5410_MFC_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_mfc); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
			exynos54xx_pwr_reg_mfc[i], 0);
	exynos_pm_add_callback(&exynos54xx_pd_mfc, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
		true, exynos5420_pm_domain_mfc_post_power_on);

	exynos_pm_add_reg(&exynos54xx_pd_gscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5_GSCL_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_gscl); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_gscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
			exynos54xx_pwr_reg_gscl[i], 0);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
		true, exynos5420_pm_domain_gscl_post_power_on);
#if 0
	/* Not implemented yet */
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos5420_pm_domain_gscl_pre_power_on);
	exynos_pm_add_callback(&exynos54xx_pd_gscl, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_OFF,
				false, exynos5420_pm_domain_gscl_post_power_off);
#endif
	exynos_pm_add_reg(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5410_MAU_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos5420_pwr_reg_mau); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
			exynos5420_pwr_reg_mau[i], 0);
	exynos_pm_add_reg(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5420_CMU_SYSCLK_MAU_SYS_PWR_REG, 1);
	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				false, exynos5_pm_domain_maudio_pre_power_off);
	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_maudio_post_power_on);
	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				false, exynos5420_pm_domain_maudio_pre_power_control);
	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos5420_pm_domain_maudio_pre_power_on);
	exynos_pm_add_callback(&exynos54xx_pd_maudio, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_OFF,
				false, exynos5420_pm_domain_maudio_post_power_off);

	exynos_pm_add_reg(&exynos54xx_pd_disp1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5410_DISP1_OPTION, 3);
	for (i = 0; i < ARRAY_SIZE(exynos5420_pwr_reg_disp1); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_disp1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
			exynos5420_pwr_reg_disp1[i], 0);
	exynos_pm_add_callback(&exynos54xx_pd_disp1, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
		true, exynos5420_pm_domain_disp1_pre_power_off);
	exynos_pm_add_callback(&exynos54xx_pd_disp1, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
		true, exynos5420_pm_domain_disp1_post_power_on);

	exynos_pm_add_reg(&exynos5420_pd_mscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
		EXYNOS5420_MSC_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos5420_pwr_reg_mscl); ++i)
		exynos_pm_add_reg(&exynos5420_pd_mscl, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
			exynos5420_pwr_reg_mscl[i], 0);
	exynos_pm_add_callback(&exynos5420_pd_mscl, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
		true, exynos5420_pm_domain_mscl_post_power_on);

	exynos_pm_add_clk(&exynos54xx_pd_mfc, &SYSMMU_PLATDEV(mfc_lr).dev, "sysmmu");
#ifdef CONFIG_S5P_DEV_MFC
	exynos_pm_add_platdev(&exynos54xx_pd_mfc, &s5p_device_mfc);
	exynos_pm_add_platdev(&exynos54xx_pd_mfc, &SYSMMU_PLATDEV(mfc_lr));
	exynos_pm_add_callback(&exynos54xx_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos5_pm_domain_bts_off);
	exynos_pm_add_callback(&exynos54xx_pd_mfc, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				false, exynos5_pm_domain_bts_on);
	exynos_pm_add_clk(&exynos54xx_pd_mfc, &s5p_device_mfc.dev, "mfc");
#else
	exynos_pm_add_clk(&exynos54xx_pd_mfc, NULL, "mfc");
#endif

#ifdef CONFIG_MALI_T6XX
	exynos_pm_add_platdev(&exynos54xx_pd_g3d, &exynos5_device_g3d);
	exynos_pm_add_clk(&exynos54xx_pd_g3d, &exynos5_device_g3d.dev, "g3d");
#else
	exynos_pm_add_clk(&exynos54xx_pd_g3d, NULL, "g3d");
#endif

	exynos_pm_add_clk(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif0).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_clk(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif1).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_clk(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(isp3).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif0));
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(camif1));
	exynos_pm_add_platdev(&exynos54xx_pd_gscl, &SYSMMU_PLATDEV(isp3));

	exynos_pm_add_clk(&exynos5420_spd_gscaler0, &SYSMMU_PLATDEV(gsc0).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_clk(&exynos5420_spd_gscaler1, &SYSMMU_PLATDEV(gsc1).dev, SYSMMU_CLOCK_NAME);
	exynos_pm_add_platdev(&exynos5420_spd_gscaler0, &SYSMMU_PLATDEV(gsc0));
	exynos_pm_add_platdev(&exynos5420_spd_gscaler1, &SYSMMU_PLATDEV(gsc1));
#ifdef CONFIG_EXYNOS5_DEV_GSC
	exynos_pm_add_platdev(&exynos5420_spd_gscaler0, &exynos5_device_gsc0);
	exynos_pm_add_platdev(&exynos5420_spd_gscaler1, &exynos5_device_gsc1);
	exynos_pm_add_clk(&exynos5420_spd_gscaler0, &exynos5_device_gsc0.dev, "gscl");
	exynos_pm_add_clk(&exynos5420_spd_gscaler1, &exynos5_device_gsc1.dev, "gscl");
	exynos_pm_add_clk(&exynos54xx_pd_disp1, &exynos5_device_gsc0.dev, "gscl");
	exynos_pm_add_clk(&exynos54xx_pd_disp1, &exynos5_device_gsc1.dev, "gscl");
#else
	exynos_pm_add_clk(&exynos5420_spd_gscaler0, NULL, "gscl");
	exynos_pm_add_clk(&exynos5420_spd_gscaler1, NULL, "gscl");
	exynos_pm_add_clk(&exynos54xx_pd_disp1, NULL, "gscl");
	exynos_pm_add_clk(&exynos54xx_pd_disp1, NULL, "gscl");
#endif

	exynos_pm_add_platdev(&exynos5420_spd_fimd1, &s5p_device_fimd1);
	exynos_pm_add_platdev(&exynos5420_spd_fimd1, &SYSMMU_PLATDEV(fimd1));
	exynos_pm_add_platdev(&exynos5420_spd_fimd1, &SYSMMU_PLATDEV(fimd1a));
	exynos_pm_add_clk(&exynos5420_spd_mixer, &SYSMMU_PLATDEV(tv).dev, "sysmmu");
	exynos_pm_add_clk(&exynos5420_spd_fimd1, &SYSMMU_PLATDEV(fimd1).dev, "sysmmu");
	exynos_pm_add_clk(&exynos5420_spd_fimd1, &SYSMMU_PLATDEV(fimd1a).dev, "sysmmu");
	exynos_pm_add_clk(&exynos5420_spd_fimd1, &s5p_device_fimd1.dev, "lcd");
#ifdef CONFIG_S5P_DEV_MIPI_DSIM1
	exynos_pm_add_clk(&exynos5420_spd_dsim1, &s5p_device_mipi_dsim1.dev, "dsim1");
#endif
	exynos_pm_add_clk(&exynos5420_spd_fimd1, NULL, "axi_disp1");
#ifdef CONFIG_FB_MIPI_DSIM
	exynos_pm_add_platdev(&exynos5420_spd_dsim1, &s5p_device_mipi_dsim1);
#endif
#ifdef CONFIG_S5P_DEV_TV
	exynos_pm_add_platdev(&exynos5420_spd_mixer, &s5p_device_mixer);
	exynos_pm_add_platdev(&exynos5420_spd_mixer, &SYSMMU_PLATDEV(tv));
	exynos_pm_add_platdev(&exynos5420_spd_hdmi, &s5p_device_hdmi);
	exynos_pm_add_clk(&exynos5420_spd_hdmi, &s5p_device_hdmi.dev, "hdmi");
	exynos_pm_add_clk(&exynos5420_spd_mixer, &s5p_device_mixer.dev, "mixer");
#else
	exynos_pm_add_clk(&exynos5420_spd_hdmi, NULL, "hdmi");
	exynos_pm_add_clk(&exynos5420_spd_mixer, NULL, "mixer");
#endif
	exynos_pm_add_callback(&exynos54xx_pd_disp1, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
		true, exynos5_pm_domain_disp1_post_power_control);
#ifdef CONFIG_S5P_DP
	exynos_pm_add_platdev(&exynos5420_spd_dp, &s5p_device_dp);
	exynos_pm_add_clk(&exynos5420_spd_dp, &s5p_device_dp.dev, "dp");
#else
	exynos_pm_add_clk(&exynos5420_spd_dp, NULL, "dp");
#endif

#ifdef CONFIG_SND_SAMSUNG_I2S
	exynos_pm_add_platdev(&exynos54xx_pd_maudio, &exynos5_device_i2s0);
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	exynos_pm_add_platdev(&exynos54xx_pd_maudio, &exynos5_device_pcm0);
#endif
#ifdef CONFIG_SND_SAMSUNG_ALP
	exynos_pm_add_platdev(&exynos54xx_pd_maudio, &exynos5_device_srp);
#endif

	exynos_pm_add_clk(&exynos5420_spd_mscl0, &SYSMMU_PLATDEV(scaler0r).dev, "sysmmu");
	exynos_pm_add_clk(&exynos5420_spd_mscl1, &SYSMMU_PLATDEV(scaler1r).dev, "sysmmu");
	exynos_pm_add_clk(&exynos5420_spd_mscl2, &SYSMMU_PLATDEV(scaler2r).dev, "sysmmu");
#ifdef CONFIG_EXYNOS5_DEV_SCALER
	exynos_pm_add_platdev(&exynos5420_spd_mscl0, &exynos5_device_scaler0);
	exynos_pm_add_platdev(&exynos5420_spd_mscl0, &SYSMMU_PLATDEV(scaler0r));
	exynos_pm_add_platdev(&exynos5420_spd_mscl0, &SYSMMU_PLATDEV(scaler0w));
	exynos_pm_add_platdev(&exynos5420_spd_mscl1, &exynos5_device_scaler1);
	exynos_pm_add_platdev(&exynos5420_spd_mscl1, &SYSMMU_PLATDEV(scaler1r));
	exynos_pm_add_platdev(&exynos5420_spd_mscl1, &SYSMMU_PLATDEV(scaler1w));
	exynos_pm_add_platdev(&exynos5420_spd_mscl2, &exynos5_device_scaler2);
	exynos_pm_add_platdev(&exynos5420_spd_mscl2, &SYSMMU_PLATDEV(scaler2r));
	exynos_pm_add_platdev(&exynos5420_spd_mscl2, &SYSMMU_PLATDEV(scaler2w));
	exynos_pm_add_clk(&exynos5420_spd_mscl0, &exynos5_device_scaler0.dev, "mscl");
	exynos_pm_add_clk(&exynos5420_spd_mscl1, &exynos5_device_scaler1.dev, "mscl");
	exynos_pm_add_clk(&exynos5420_spd_mscl2, &exynos5_device_scaler2.dev, "mscl");
#else
	exynos_pm_add_clk(&exynos5420_spd_mscl0, NULL, "mscl");
	exynos_pm_add_clk(&exynos5420_spd_mscl1, NULL, "mscl");
	exynos_pm_add_clk(&exynos5420_spd_mscl2, NULL, "mscl");
#endif
#ifdef CONFIG_EXYNOS5_DEV_FIMC_IS
	exynos_pm_powerdomain_init(&exynos54xx_pd_isp);
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &exynos5_device_fimc_is);
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &s3c64xx_device_spi3);
	exynos_pm_add_clk(&exynos54xx_pd_isp, &exynos5_device_fimc_is.dev, "gscl_wrap0");
	exynos_pm_add_clk(&exynos54xx_pd_isp, &exynos5_device_fimc_is.dev, "gscl_wrap1");
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_pd_isp, false);
	exynos54xx_pd_isp.on	= exynos5410_pm_domain_isp_power_control;
	exynos54xx_pd_isp.off	= exynos5410_pm_domain_isp_power_control;

	for (i = 0; i < ARRAY_SIZE(exynos54xx_pwr_reg_isp); ++i)
		exynos_pm_add_reg(&exynos54xx_pd_isp, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				exynos54xx_pwr_reg_isp[i], 0);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
	exynos_pm_powerdomain_init(&exynos54xx_spd_mipi_csis0);
	exynos_pm_add_platdev(&exynos54xx_spd_mipi_csis0, &s5p_device_mipi_csis0);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_mipi_csis0, true);

	exynos_pm_powerdomain_init(&exynos54xx_spd_mipi_csis1);
	exynos_pm_add_platdev(&exynos54xx_spd_mipi_csis1, &s5p_device_mipi_csis1);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_mipi_csis1, true);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	exynos_pm_powerdomain_init(&exynos54xx_spd_flite0);
	exynos_pm_add_platdev(&exynos54xx_spd_flite0, &exynos_device_flite0);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_flite0, true);

	exynos_pm_powerdomain_init(&exynos54xx_spd_flite1);
	exynos_pm_add_platdev(&exynos54xx_spd_flite1, &exynos_device_flite1);
	exynos_pm_add_subdomain(&exynos54xx_pd_gscl, &exynos54xx_spd_flite1, true);
#endif
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &SYSMMU_PLATDEV(isp0));
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &SYSMMU_PLATDEV(isp1));
	exynos_pm_add_platdev(&exynos54xx_pd_isp, &SYSMMU_PLATDEV(isp2));

	exynos5420_bts_initialize();

	return 0;
}

int __init exynos5_pm_domain_init(void)
{
	regs = ioremap(0x14530000, SZ_32);
	if (IS_ERR_OR_NULL(regs))
		pr_err("PM DOMAIN : can't remap of hdmi phy address\n");

	exynos_pm_powerdomain_init(&exynos54xx_pd_mfc);
	exynos_pm_powerdomain_init(&exynos54xx_pd_g3d);
	exynos_pm_powerdomain_init(&exynos54xx_pd_maudio);
	exynos_pm_powerdomain_init(&exynos54xx_pd_disp1);
	exynos_pm_powerdomain_init(&exynos54xx_pd_gscl);

	if (soc_is_exynos5410()) {
		return exynos5410_pm_domain_init();
	} else if (soc_is_exynos5420()) {
		return exynos5420_pm_domain_init();
	} else {
		pr_err("PM DOMAIN : can't find soc information\n");
		return -EINVAL;
	}

	return 0;
}
arch_initcall(exynos5_pm_domain_init);
