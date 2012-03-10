/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_domain.h>
#include <linux/delay.h>
#include <linux/of_address.h>

#include <mach/regs-pmu.h>
#include <plat/cpu.h>
#include <plat/devs.h>

/*
 * Exynos specific wrapper around the generic power domain
 */
struct exynos_pm_domain {
	void __iomem *base;
	char const *name;
	bool is_off;
	struct generic_pm_domain pd;
};

static int exynos_pd_power(struct generic_pm_domain *domain, bool power_on)
{
	struct exynos_pm_domain *pd;
	void __iomem *base;
	u32 timeout, pwr;
	char *op;

	pd = container_of(domain, struct exynos_pm_domain, pd);
	base = pd->base;

	pwr = power_on ? EXYNOS_INT_LOCAL_PWR_EN : 0;
	__raw_writel(pwr, base);

	/* Wait max 1ms */
	timeout = 10;

	while ((__raw_readl(base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) != pwr) {
		if (!timeout) {
			op = (power_on) ? "enable" : "disable";
			pr_err("Power domain %s %s failed\n", domain->name, op);
			return -ETIMEDOUT;
		}
		timeout--;
		cpu_relax();
		usleep_range(80, 100);
	}
	return 0;
}

static int exynos_pd_power_on(struct generic_pm_domain *domain)
{
	return exynos_pd_power(domain, true);
}

static int exynos_pd_power_off(struct generic_pm_domain *domain)
{
	return exynos_pd_power(domain, false);
}

#define EXYNOS_GPD(PD, BASE, NAME)			\
static struct exynos_pm_domain PD = {			\
	.base = (void __iomem *)BASE,			\
	.name = NAME,					\
	.pd = {						\
		.power_off = exynos_pd_power_off,	\
		.power_on = exynos_pd_power_on,	\
	},						\
}

#ifdef CONFIG_OF
static __init int exynos_pm_dt_parse_domains(void)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "samsung,exynos4210-pd") {
		struct exynos_pm_domain *pd;

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			pr_err("%s: failed to allocate memory for domain\n",
					__func__);
			return -ENOMEM;
		}

		if (of_get_property(np, "samsung,exynos4210-pd-off", NULL))
			pd->is_off = true;
		pd->name = np->name;
		pd->base = of_iomap(np, 0);
		pd->pd.power_off = exynos_pd_power_off;
		pd->pd.power_on = exynos_pd_power_on;
		pd->pd.of_node = np;
		pm_genpd_init(&pd->pd, NULL, false);
	}
	return 0;
}
#else
static __init int exynos_pm_dt_parse_domains(void)
{
	return 0;
}
#endif /* CONFIG_OF */

static __init void exynos_pm_add_dev_to_genpd(struct platform_device *pdev,
						struct exynos_pm_domain *pd)
{
	if (pdev->dev.bus) {
		if (pm_genpd_add_device(&pd->pd, &pdev->dev))
			pr_info("%s: error in adding %s device to %s power"
				"domain\n", __func__, dev_name(&pdev->dev),
				pd->name);
	}
}

/* For EXYNOS4 */
EXYNOS_GPD(exynos4_pd_mfc, EXYNOS4_MFC_CONFIGURATION, "pd-mfc");
EXYNOS_GPD(exynos4_pd_g3d, EXYNOS4_G3D_CONFIGURATION, "pd-g3d");
EXYNOS_GPD(exynos4_pd_lcd0, EXYNOS4_LCD0_CONFIGURATION, "pd-lcd0");
EXYNOS_GPD(exynos4_pd_tv, EXYNOS4_TV_CONFIGURATION, "pd-tv");
EXYNOS_GPD(exynos4_pd_cam, EXYNOS4_CAM_CONFIGURATION, "pd-cam");
EXYNOS_GPD(exynos4_pd_gps, EXYNOS4_GPS_CONFIGURATION, "pd-gps");

/* For EXYNOS4210 */
EXYNOS_GPD(exynos4210_pd_lcd1, EXYNOS4210_LCD1_CONFIGURATION, "pd-lcd1");

static struct exynos_pm_domain *exynos4_pm_domains[] = {
	&exynos4_pd_mfc,
	&exynos4_pd_g3d,
	&exynos4_pd_lcd0,
	&exynos4_pd_tv,
	&exynos4_pd_cam,
	&exynos4_pd_gps,
};

static struct exynos_pm_domain *exynos4210_pm_domains[] = {
	&exynos4210_pd_lcd1,
};

static __init int exynos4_pm_init_power_domain(void)
{
	int idx;

	if (of_have_populated_dt())
		return exynos_pm_dt_parse_domains();

	for (idx = 0; idx < ARRAY_SIZE(exynos4_pm_domains); idx++)
		pm_genpd_init(&exynos4_pm_domains[idx]->pd, NULL,
				exynos4_pm_domains[idx]->is_off);

	if (soc_is_exynos4210())
		for (idx = 0; idx < ARRAY_SIZE(exynos4210_pm_domains); idx++)
			pm_genpd_init(&exynos4210_pm_domains[idx]->pd, NULL,
					exynos4210_pm_domains[idx]->is_off);

#ifdef CONFIG_S5P_DEV_FIMD0
	exynos_pm_add_dev_to_genpd(&s5p_device_fimd0, &exynos4_pd_lcd0);
#endif
#ifdef CONFIG_S5P_DEV_TV
	exynos_pm_add_dev_to_genpd(&s5p_device_hdmi, &exynos4_pd_tv);
	exynos_pm_add_dev_to_genpd(&s5p_device_mixer, &exynos4_pd_tv);
#endif
#ifdef CONFIG_S5P_DEV_MFC
	exynos_pm_add_dev_to_genpd(&s5p_device_mfc, &exynos4_pd_mfc);
#endif
#ifdef CONFIG_S5P_DEV_FIMC0
	exynos_pm_add_dev_to_genpd(&s5p_device_fimc0, &exynos4_pd_cam);
#endif
#ifdef CONFIG_S5P_DEV_FIMC1
	exynos_pm_add_dev_to_genpd(&s5p_device_fimc1, &exynos4_pd_cam);
#endif
#ifdef CONFIG_S5P_DEV_FIMC2
	exynos_pm_add_dev_to_genpd(&s5p_device_fimc2, &exynos4_pd_cam);
#endif
#ifdef CONFIG_S5P_DEV_FIMC3
	exynos_pm_add_dev_to_genpd(&s5p_device_fimc3, &exynos4_pd_cam);
#endif
#ifdef CONFIG_S5P_DEV_CSIS0
	exynos_pm_add_dev_to_genpd(&s5p_device_mipi_csis0, &exynos4_pd_cam);
#endif
#ifdef CONFIG_S5P_DEV_CSIS1
	exynos_pm_add_dev_to_genpd(&s5p_device_mipi_csis1, &exynos4_pd_cam);
#endif
#ifdef CONFIG_S5P_DEV_G2D
	exynos_pm_add_dev_to_genpd(&s5p_device_g2d, &exynos4_pd_lcd0);
#endif
#ifdef CONFIG_S5P_DEV_JPEG
	exynos_pm_add_dev_to_genpd(&s5p_device_jpeg, &exynos4_pd_cam);
#endif
	return 0;
}

/* For EXYNOS5 */
EXYNOS_GPD(exynos5_pd_mfc, EXYNOS5_MFC_CONFIGURATION, "pd-mfc");
EXYNOS_GPD(exynos5_pd_gscl, EXYNOS5_GSCL_CONFIGURATION, "pd-gscl");

static struct exynos_pm_domain *exynos5_pm_domains[] = {
	&exynos5_pd_mfc,
	&exynos5_pd_gscl,
};

static int __init exynos5_pm_init_power_domain(void)
{
	int idx;

	if (of_have_populated_dt())
		return exynos_pm_dt_parse_domains();

	for (idx = 0; idx < ARRAY_SIZE(exynos5_pm_domains); idx++)
		pm_genpd_init(&exynos5_pm_domains[idx]->pd, NULL,
				exynos5_pm_domains[idx]->is_off);

#ifdef CONFIG_S5P_DEV_MFC
	exynos_pm_add_dev_to_genpd(&s5p_device_mfc, &exynos5_pd_mfc);
#endif
#ifdef CONFIG_EXYNOS5_DEV_GSC
	exynos_pm_add_dev_to_genpd(&exynos5_device_gsc0, &exynos5_pd_gscl);
	exynos_pm_add_dev_to_genpd(&exynos5_device_gsc1, &exynos5_pd_gscl);
	exynos_pm_add_dev_to_genpd(&exynos5_device_gsc2, &exynos5_pd_gscl);
	exynos_pm_add_dev_to_genpd(&exynos5_device_gsc3, &exynos5_pd_gscl);
#endif

	return 0;
}

static int __init exynos_pm_init_power_domain(void)
{
	if (soc_is_exynos5250())
		return exynos5_pm_init_power_domain();
	else
		return exynos4_pm_init_power_domain();
}
arch_initcall(exynos_pm_init_power_domain);

static __init int exynos_pm_late_initcall(void)
{
	pm_genpd_poweroff_unused();
	return 0;
}
late_initcall(exynos_pm_late_initcall);
