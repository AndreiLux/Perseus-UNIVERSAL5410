/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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

#include <mach/pm_domains.h>
#include <mach/pm_interrupt_domains.h>

void exynos_pm_powerdomain_init(struct exynos_pm_domain *domain)
{
	if (domain->is_interrupt_domain)
		exynos_pm_interrupt_powerdomain_init(domain, false);
	else
		pm_genpd_init(&domain->pd, NULL, false);

	exynos_pm_add_callback(domain, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ONOFF,
				true, exynos_pm_domain_pre_power_control);
	exynos_pm_add_callback(domain, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ONOFF,
				true, exynos_pm_domain_post_power_control);
}

static int exynos_pm_domain_dummy_callback(struct exynos_pm_domain *domain,
						int power_flags)
{
	return 0;
}

void exynos_pm_domain_set_nocallback(struct exynos_pm_domain *domain)
{
	domain->on = exynos_pm_domain_dummy_callback;
	domain->off = exynos_pm_domain_dummy_callback;
}

void exynos_pm_add_subdomain(struct exynos_pm_domain *domain,
				struct exynos_pm_domain *subdomain,
				bool logical_subdomain)
{
	if (domain == NULL) {
		pr_err("PM DOMAIN : cant add subdomain, domain is empty\n");
		return;
	}

	if (subdomain == NULL) {
		pr_err("PM DOMAIN : cant add subdomain, subdomain is emptry\n");
		return;
	}

	if (domain->is_interrupt_domain || subdomain->is_interrupt_domain) {
		pr_err("PM DOMAIN : %s -> %s interrupt power domain does not support sub domain\n",
				domain->pd.name, subdomain->pd.name);
		return;
	}

	if (logical_subdomain)
		exynos_pm_domain_set_nocallback(subdomain);

	if (pm_genpd_add_subdomain(&domain->pd, &subdomain->pd))
		pr_err("PM DOMAIN : %s cant add subdomain %s\n", domain->pd.name, subdomain->pd.name);
}

void exynos_pm_add_dev(struct exynos_pm_domain *domain,
			struct device *dev)
{
	int ret = 0;

	if (domain == NULL) {
		pr_err("PM DOMAIN : cant add device, domain is empty\n");
		return;
	}

	if (dev == NULL) {
		pr_err("PM DOMAIN : cant add device, device is empty\n");
		return;
	}

	if (!domain->is_interrupt_domain) {
		ret = pm_genpd_add_device(&domain->pd, dev);
		if (!ret)
			pm_genpd_dev_need_restore(dev, true);
	} else {
		ret = exynos_pm_interrupt_add_device(domain, dev);
		if (!ret)
			exynos_pm_interrupt_need_restore(dev, true);
	}

	if (!ret)
		pr_info("PM DOMAIN : %s, Device : %s Registered\n", domain->pd.name, dev_name(dev));
	else
		pr_err("PM DOMAIN : %s cant add device %s\n", domain->pd.name, dev_name(dev));
}

void exynos_pm_add_platdev(struct exynos_pm_domain *domain,
				struct platform_device *pdev)
{
	if (pdev == NULL) {
		pr_err("PM DOMAIN : cant add device, platform_device is empty\n");
		return;
	}

	if (pdev->dev.bus == NULL) {
		pr_err("PM DOMAIN : platform_device has not platform bus\n");
		return;
	}

	exynos_pm_add_dev(domain, &pdev->dev);
}

void exynos_pm_add_clk(struct exynos_pm_domain *domain,
			struct device *dev,
			char *con_id)
{
	struct exynos_pm_clk *exynos_clk = NULL;
	struct clk *clk = NULL;

	if (domain == NULL) {
		pr_err("PM DOMAIN : cant add clock, domain is empty\n");
		return;
	}

	exynos_clk = kzalloc(sizeof(struct exynos_pm_clk), GFP_KERNEL);
	if (exynos_clk == NULL) {
		pr_err("PM DOMAIN : cant allocation clock, no free memory\n");
		return;
	}

	clk = clk_get(dev, con_id);
	if (IS_ERR(clk)) {
		pr_err("PM DOMAIN : cant find clock, to add power domain\n");
		kfree(exynos_clk);
		return;
	}

	exynos_clk->clk = clk;

	list_add(&exynos_clk->node, &domain->clk_list);
}

void exynos_pm_add_reg(struct exynos_pm_domain *domain,
			enum EXYNOS_PROCESS_ORDER reg_order,
			enum EXYNOS_PROCESS_TYPE reg_type,
			void __iomem *reg,
			unsigned int value)
{
	struct exynos_pm_reg *exynos_reg = NULL;
	struct list_head *exynos_list_head = NULL;

	if (domain == NULL) {
		pr_err("PM DOMAIN : cant add register, domain is empty\n");
		return;
	}

	if (reg == NULL) {
		pr_err("PM DOMAIN : cant add register, register is empty\n");
		return;
	}

	exynos_reg = kzalloc(sizeof(struct exynos_pm_reg), GFP_KERNEL);
	if (exynos_reg == NULL) {
		pr_err("PM DOMAIN : cant allocation register, no free memory\n");
		return;
	}

	exynos_reg->reg_type = reg_type;
	exynos_reg->reg = reg;
	exynos_reg->value = value;

	if (reg_order & EXYNOS_PROCESS_BEFORE)
		exynos_list_head = &domain->reg_before_list;

	if (reg_order & EXYNOS_PROCESS_AFTER)
		exynos_list_head = &domain->reg_after_list;

	if (exynos_list_head != NULL) {
		list_add(&exynos_reg->node, exynos_list_head);
	} else {
		pr_info("PM DOMAIN : register dont add anywhere\n");
		kfree(exynos_reg);
	}
}

void exynos_pm_add_callback(struct exynos_pm_domain *domain,
			enum EXYNOS_PROCESS_ORDER callback_order,
			enum EXYNOS_PROCESS_TYPE callback_type,
			bool add_tail,
			int (*callback)(struct exynos_pm_domain *domain))
{
	struct exynos_pm_callback *exynos_callback_on = NULL;
	struct exynos_pm_callback *exynos_callback_off = NULL;

	if (domain == NULL) {
		pr_err("PM DOMAIN : cant add callback, domain is empty\n");
		return;
	}

	if (callback == NULL) {
		pr_err("PM DOMAIN : cant add callback, callback is empty\n");
		return;
	}

	if (callback_type & EXYNOS_PROCESS_ON) {
		exynos_callback_on = kzalloc(sizeof(struct exynos_pm_callback), GFP_KERNEL);
		if (exynos_callback_on == NULL) {
			pr_err("PM DOMAIN : cant allocation callback, no free memory\n");
			goto err;
		}

		exynos_callback_on->callback = callback;
	}

	if (callback_type & EXYNOS_PROCESS_OFF) {
		exynos_callback_off = kzalloc(sizeof(struct exynos_pm_callback), GFP_KERNEL);
		if (exynos_callback_off == NULL) {
			pr_err("PM DOMAIN : cant allocation callback, no free memory\n");
			goto err;
		}

		exynos_callback_off->callback = callback;
	}

	if (callback_order & EXYNOS_PROCESS_BEFORE) {
		if (callback_type & EXYNOS_PROCESS_ON) {
			if (add_tail)
				list_add_tail(&exynos_callback_on->node,
						&domain->callback_pre_on_list);
			else
				list_add(&exynos_callback_on->node,
						&domain->callback_pre_on_list);
		}
		if (callback_type & EXYNOS_PROCESS_OFF) {
			if (add_tail)
				list_add_tail(&exynos_callback_off->node,
						&domain->callback_pre_off_list);
			else
				list_add(&exynos_callback_off->node,
						&domain->callback_pre_off_list);
		}
	}

	if (callback_order & EXYNOS_PROCESS_AFTER) {
		if (callback_type & EXYNOS_PROCESS_ON) {
			if (add_tail)
				list_add_tail(&exynos_callback_on->node,
					&domain->callback_post_on_list);
			else
				list_add(&exynos_callback_on->node,
					&domain->callback_post_on_list);
		}
		if (callback_type & EXYNOS_PROCESS_OFF) {
			if (add_tail)
				list_add_tail(&exynos_callback_off->node,
					&domain->callback_post_off_list);
			else
				list_add(&exynos_callback_off->node,
					&domain->callback_post_off_list);
		}
	}

	return;
err:
	kfree(exynos_callback_on);
	kfree(exynos_callback_off);
}

void print_reg_pm_disp1(void)
{
        pr_info("PMU_CG_STATUS0         [0x%x]\n", __raw_readl(EXYNOS_PMU_CG_STATUS0));
        pr_info("PMU_CG_STATUS1         [0x%x]\n", __raw_readl(EXYNOS_PMU_CG_STATUS1));
        pr_info("PMU_CG_STATUS2         [0x%x]\n", __raw_readl(EXYNOS_PMU_CG_STATUS2));
        pr_info("PMU_CG_STATUS3         [0x%x]\n", __raw_readl(EXYNOS_PMU_CG_STATUS3));
        pr_info("PMU_CG_STATUS4         [0x%x]\n", __raw_readl(EXYNOS_PMU_CG_STATUS4));

        pr_info("CMU_CLKSTOP_DISP1_SYS_PWR_REG  [0x%x]\n", __raw_readl(EXYNOS5420_CMU_CLKSTOP_DISP1_SYS_PWR_REG));
        pr_info("CMU_SYSCLK_DISP1_SYS_PWR_REG   [0x%x]\n", __raw_readl(EXYNOS5420_CMU_SYSCLK_DISP1_SYS_PWR_REG));
        pr_info("CMU_RESET_DISP1_SYS_PWR_REG    [0x%x]\n", __raw_readl(EXYNOS5420_CMU_RESET_DISP1_SYS_PWR_REG));
        pr_info("DISP1_STATUS                   [0x%x]\n", __raw_readl(EXYNOS5410_DISP1_STATUS));

        pr_info("CLK_SRC_DISP10         [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_DISP1_0));
        pr_info("CLK_SRC_TOP3           [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_TOP3));
        pr_info("CLK_SRC_TOP5           [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_TOP5));
        pr_info("CLK_SRC_MASK_TOP0      [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_TOP0));
        pr_info("CLK_SRC_MASK_TOP1      [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_TOP1));
        pr_info("CLK_SRC_MASK_TOP2      [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_TOP2));
        pr_info("CLK_SRC_MASK_DISP10    [0x%x]\n", __raw_readl(EXYNOS5_CLKSRC_MASK_DISP1_0));

        pr_info("CLK_DIV_TOP2           [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_TOP2));
        pr_info("CLK_DIV_DISP10         [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV_DISP1_0));
        pr_info("CLK_DIV2_RATIO0        [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV2_RATIO0));
        pr_info("CLK_DIV2_RATIO1        [0x%x]\n", __raw_readl(EXYNOS5_CLKDIV2_RATIO1));

        pr_info("CLK_GATE_BUS_TOP       [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BUS_TOP));
        pr_info("CLK_GATE_BUS_DISP1     [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BUS_DISP1));
        pr_info("CLK_GATE_BUS_NOC       [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BUS_NOC));
        pr_info("CLK_GATE_IP_DISP1      [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_IP_DISP1));
        pr_info("CLK_GATE_BLOCK         [0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_BLOCK));
        pr_info("CLK_GATE_TOP_SCLK_DISP1[0x%x]\n", __raw_readl(EXYNOS5_CLKGATE_TOP_SCLK_DISP1));
}

int exynos_pm_genpd_power_on(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *domain = container_of(genpd, struct exynos_pm_domain, pd);
	struct exynos_pm_callback *exynos_callback;
	struct exynos_pm_reg *exynos_reg;
	int ret;

	if (domain->on == NULL) {
		pr_err("PM DOMAIN : %s cant support power on!\n", genpd->name);
		return -EINVAL;
	}

	list_for_each_entry(exynos_callback, &domain->callback_pre_on_list, node) {
		ret = exynos_callback->callback(domain);
		if (ret) {
			pr_err("PM DOMAIN : %s occur error at pre power on!\n", genpd->name);
			return ret;
		}
	}

	list_for_each_entry(exynos_reg, &domain->reg_before_list, node) {
		if (exynos_reg->reg_type & EXYNOS_PROCESS_ON)
			__raw_writel(exynos_reg->value, exynos_reg->reg);
	}

	ret = domain->on(domain, EXYNOS_INT_LOCAL_PWR_EN);
	if (ret) {
		pr_err("PM DOMAIN : %s occur error at power on!\n", genpd->name);
		return ret;
	}

	list_for_each_entry(exynos_reg, &domain->reg_after_list, node) {
		if (exynos_reg->reg_type & EXYNOS_PROCESS_ON)
			__raw_writel(exynos_reg->value, exynos_reg->reg);
	}

	list_for_each_entry(exynos_callback, &domain->callback_post_on_list, node) {
		ret = exynos_callback->callback(domain);
		if (ret) {
			pr_err("PM DOMAIN : %s occur error at post power on!\n", genpd->name);
			return ret;
		}
	}

	return 0;
}

int exynos_pm_genpd_power_off(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *domain = container_of(genpd, struct exynos_pm_domain, pd);
	struct exynos_pm_callback *exynos_callback;
	struct exynos_pm_reg *exynos_reg;
	int ret;

	if (domain->off == NULL) {
		pr_err("PM DOMAIN : %s cant support power off!\n", genpd->name);
		return -EINVAL;
	}

	list_for_each_entry(exynos_callback, &domain->callback_pre_off_list, node) {
		ret = exynos_callback->callback(domain);
		if (ret) {
			pr_err("PM DOMAIN : %s occur error at pre power off!\n", genpd->name);
			return ret;
		}
	}

	list_for_each_entry(exynos_reg, &domain->reg_before_list, node) {
		if (exynos_reg->reg_type & EXYNOS_PROCESS_OFF)
			__raw_writel(exynos_reg->value, exynos_reg->reg);
	}

	ret = domain->off(domain, 0);
	if (ret) {
		pr_err("PM DOMAIN : %s occur error at power off!\n", genpd->name);
		return ret;
	}

	list_for_each_entry(exynos_reg, &domain->reg_after_list, node) {
		if (exynos_reg->reg_type & EXYNOS_PROCESS_OFF)
			__raw_writel(exynos_reg->value, exynos_reg->reg);
	}

	list_for_each_entry(exynos_callback, &domain->callback_post_off_list, node) {
		ret = exynos_callback->callback(domain);
		if (ret) {
			pr_err("PM DOMAIN : %s occur error at post power off!\n", genpd->name);
			return ret;
		}
	}

	return 0;
}

int exynos_pm_domain_pre_power_control(struct exynos_pm_domain *domain)
{
	struct exynos_pm_domain *subdomain;
	struct exynos_pm_clk *pclk;
	struct gpd_link *domain_link;

	list_for_each_entry(pclk, &domain->clk_list, node) {
		if (clk_enable(pclk->clk)) {
			list_for_each_entry_continue_reverse(pclk, &domain->clk_list, node)
				clk_disable(pclk->clk);
			return -EINVAL;
		}
	}

	if (!domain->is_interrupt_domain) {
		list_for_each_entry(domain_link, &domain->pd.master_links, master_node) {
			subdomain = container_of(domain_link->slave, struct exynos_pm_domain, pd);
			if (exynos_pm_domain_pre_power_control(subdomain)) {
				list_for_each_entry_continue_reverse(domain_link, &domain->pd.master_links, master_node) {
					subdomain = container_of(domain_link->slave, struct exynos_pm_domain, pd);
					exynos_pm_domain_post_power_control(subdomain);
				}
				list_for_each_entry(pclk, &domain->clk_list, node)
					clk_disable(pclk->clk);
				return -EINVAL;
			}
		}
	}

	return 0;
}

int exynos_pm_domain_post_power_control(struct exynos_pm_domain *domain)
{
	struct exynos_pm_domain *subdomain;
	struct exynos_pm_clk *pclk;
	struct gpd_link *domain_link;

	if (!domain->is_interrupt_domain) {
		list_for_each_entry(domain_link, &domain->pd.master_links, master_node) {
			subdomain = container_of(domain_link->slave, struct exynos_pm_domain, pd);
			exynos_pm_domain_post_power_control(subdomain);
		}
	}

	list_for_each_entry(pclk, &domain->clk_list, node)
		clk_disable(pclk->clk);

	return 0;
}

int exynos_pm_domain_power_control(struct exynos_pm_domain *domain,
					int power_flags)
{
	unsigned long timeout;

	if (!domain)
		return -EINVAL;

	if (domain->base != NULL) {
		__raw_writel(power_flags, domain->base);

		/* Wait max 1ms */
		timeout = 10;
		while ((__raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
			if (timeout == 0) {
				pr_err("PM DOMAIN : %s cant control power, timeout\n", domain->pd.name);
				return -ETIMEDOUT;
			}
			--timeout;
			cpu_relax();
			if (!domain->is_interrupt_domain)
				usleep_range(80, 100);
			else
				udelay(100);
		}
	}

	return 0;
}

static __init int exynos_pm_domain_idle(void)
{
	pm_genpd_poweroff_unused();

	return 0;
}
late_initcall(exynos_pm_domain_idle);
