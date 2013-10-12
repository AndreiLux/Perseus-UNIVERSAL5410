/*
 * Exynos Interrupt power domain support.
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
#ifndef __ASM_ARCH_PM_INTERRUPT_RUNTIME_H
#define __ASM_ARCH_PM_INTERRUPT_RUNTIME_H __FILE__

#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <mach/pm_domains.h>

struct exynos_pm_interrupt_domain_data {
	struct pm_domain_data base;
	struct gpd_dev_ops ops;
	spinlock_t lock;
	bool need_restore;
};

#ifdef CONFIG_PM_RUNTIME
extern int exynos_pm_interrupt_runtime_get_sync(struct device *dev);
extern int exynos_pm_interrupt_runtime_put_sync(struct device *dev);
extern void exynos_pm_interrupt_runtime_forbid(struct device *dev);
extern void exynos_pm_interrupt_runtime_allow(struct device *dev);
#else
static int exynos_pm_interrupt_runtime_get_sync(struct device *dev) { return 0; }
static int exynos_pm_interrupt_runtime_put_sync(struct device *dev) { return 0; }
static void exynos_pm_interrupt_runtime_forbid(struct device *dev) {}
static void exynos_pm_interrupt_runtime_allow(struct device *dev) {}
#endif

extern void exynos_pm_interrupt_powerdomain_init(struct exynos_pm_domain *domain, bool is_off);
extern int exynos_pm_interrupt_add_device(struct exynos_pm_domain *domain, struct device *dev);
extern void exynos_pm_interrupt_need_restore(struct device *dev, bool val);


#define EXYNOS_INTERRUPT_GPD(PD, BASE, NAME)						\
	EXYNOS_GPD(PD,									\
		BASE,									\
		NAME,									\
		exynos_pm_domain_power_control,						\
		exynos_pm_domain_power_control,						\
		exynos_pm_genpd_power_on,						\
		exynos_pm_genpd_power_off,						\
		true									\
	)

#endif /* __ASM_ARCH_PM_INTERRUPT_RUNTIME_H */
