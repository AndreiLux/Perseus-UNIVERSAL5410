/* linux/arch/arm/mach-exynos/ppmu.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - CPU PPMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/math64.h>

#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/ppmu.h>

static LIST_HEAD(ppmu_list);

unsigned long long ppmu_load[PPMU_END];

void exynos_ppmu_reset(struct exynos_ppmu_hw *ppmu)
{
	writel(0x3 << 1, ppmu->hw_base);
	writel(0x8000000f, ppmu->hw_base + PPMU_CNTENS);
}

void exynos_ppmu_setevent(struct exynos_ppmu_hw *ppmu,
				unsigned int evt_num)
{
	writel(ppmu->event[evt_num],
			ppmu->hw_base + PPMU_BEVTSEL(evt_num));
}

void exynos_ppmu_start(struct exynos_ppmu_hw *ppmu)
{
	writel(0x1, ppmu->hw_base);
}

void exynos_ppmu_stop(struct exynos_ppmu_hw *ppmu)
{
	writel(0x0, ppmu->hw_base);
}

unsigned long long exynos_ppmu_update(struct exynos_ppmu_hw *ppmu, int ch)
{
	unsigned long long total = 0;

	ppmu->ccnt = readl(ppmu->hw_base + PPMU_CCNT);

	if (ppmu->ccnt == 0)
		ppmu->ccnt = MAX_CCNT;

	if (ch >= PPMU_NR_COUNTERS || ppmu->event[ch] == 0)
		return 0;

	if (ch == 3)
		total = (((u64)readl(ppmu->hw_base +
		PMCNT_OFFSET(ch)) << 8) | readl(ppmu->hw_base +
		PMCNT_OFFSET(ch + 1)));
	else
		total = readl(ppmu->hw_base + PMCNT_OFFSET(ch));

	if (total > ppmu->ccnt)
		total = ppmu->ccnt;

	return div64_u64((total * ppmu->weight * 100), ppmu->ccnt);
}

void ppmu_start(struct device *dev)
{
	struct exynos_ppmu_hw *ppmu;

	list_for_each_entry(ppmu, &ppmu_list, node)
		if (ppmu->dev == dev)
			exynos_ppmu_start(ppmu);
}

void ppmu_update(struct device *dev, int ch)
{
	struct exynos_ppmu_hw *ppmu;

	list_for_each_entry(ppmu, &ppmu_list, node)
		if (ppmu->dev == dev) {
			exynos_ppmu_stop(ppmu);
			ppmu_load[ppmu->id] = exynos_ppmu_update(ppmu, ch);
			exynos_ppmu_reset(ppmu);
		}
}

void ppmu_reset(struct device *dev)
{
	struct exynos_ppmu_hw *ppmu;
	int i;

	list_for_each_entry(ppmu, &ppmu_list, node) {
		if (ppmu->dev == dev) {
			exynos_ppmu_stop(ppmu);
			for (i = 0; i < PPMU_NR_COUNTERS; i++)
				if (ppmu->event[i] != 0)
					exynos_ppmu_setevent(ppmu, i);
			exynos_ppmu_reset(ppmu);
		}
	}
}

void ppmu_init(struct exynos_ppmu_hw *ppmu, struct device *dev)
{
	int i;

	ppmu->hw_base = ioremap(ppmu->base_addr, SZ_8K);
	if (ppmu->hw_base == NULL) {
		printk(KERN_ERR "failed ioremap for ppmu\n");
		return;
	}

	ppmu->dev = dev;
	list_add(&ppmu->node, &ppmu_list);

	for (i = 0; i < PPMU_NR_COUNTERS; i++)
		if (ppmu->event[i] != 0)
			exynos_ppmu_setevent(ppmu, i);
}

struct exynos_ppmu_hw exynos_ppmu[] = {
	[PPMU_CPU] = {
		.id = PPMU_CPU,
		.base_addr = EXYNOS5_PA_PPMU_CPU,
		.event[3] = RDWR_DATA_COUNT,
		.weight = DEFAULT_WEIGHT,
	},
	[PPMU_DDR_C] = {
		.id = PPMU_DDR_C,
		.base_addr = EXYNOS5_PA_PPMU_DDR_C,
		.event[3] = RDWR_DATA_COUNT,
		.weight = DEFAULT_WEIGHT,
	},
	[PPMU_DDR_R1] = {
		.id = PPMU_DDR_R1,
		.base_addr = EXYNOS5_PA_PPMU_DDR_R1,
		.event[3] = RDWR_DATA_COUNT,
		.weight = DEFAULT_WEIGHT,
	},
	[PPMU_DDR_L] = {
		.id = PPMU_DDR_L,
		.base_addr = EXYNOS5_PA_PPMU_DDR_L,
		.event[3] = RDWR_DATA_COUNT,
		.weight = DEFAULT_WEIGHT,
	},
	[PPMU_RIGHT0_BUS] = {
		.id = PPMU_RIGHT0_BUS,
		.base_addr = EXYNOS5_PA_PPMU_RIGHT0_BUS,
		.event[3] = RDWR_DATA_COUNT,
		.weight = DEFAULT_WEIGHT,
	},
};
