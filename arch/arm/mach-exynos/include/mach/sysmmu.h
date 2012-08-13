/* linux/arch/arm/mach-exynos/include/mach/sysmmu.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung sysmmu driver for EXYNOS4
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARM_ARCH_SYSMMU_H
#define __ASM_ARM_ARCH_SYSMMU_H __FILE__
#include <linux/device.h>
#include <linux/pm_runtime.h>

#define SYSMMU_CLOCK_NAME "sysmmu"
#define SYSMMU_CLOCK_NAME2 "sysmmu_mc"
#define SYSMMU_DEVNAME_BASE "s5p-sysmmu"
#define SYSMMU_CLOCK_DEVNAME(ipname, id) (SYSMMU_DEVNAME_BASE "." #id)

struct sysmmu_platform_data {
	char *dbgname;
	char *clockname;
};

static inline void platform_set_sysmmu(
		struct device *sysmmu, struct device *dev)
{
	dev->archdata.iommu = sysmmu;
}

#ifdef CONFIG_EXYNOS_IOMMU
static inline int platform_sysmmu_on(struct device *dev)
{
	return pm_runtime_get_sync(dev->archdata.iommu);
}
static inline int platform_sysmmu_off(struct device *dev)
{
	return pm_runtime_put_sync(dev->archdata.iommu);
}
#else
static inline int platform_sysmmu_on(struct device *dev)
{
	return 0;
}
static inline int platform_sysmmu_off(struct device *dev)
{
	return 0;
}
#endif

struct dma_iommu_mapping *s5p_create_iommu_mapping(struct device *client,
				dma_addr_t base, unsigned int size, int order,
				struct dma_iommu_mapping *mapping);
void s5p_destroy_iommu_mapping(struct device *client);
struct platform_device *find_sysmmu_dt(struct platform_device *pdev,
					char *name);

#endif /* __ASM_ARM_ARCH_SYSMMU_H */
