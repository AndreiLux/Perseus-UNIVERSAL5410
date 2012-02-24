/* linux/arch/arm/mach-exynos/include/mach/ppmu.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PPMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_PPMU_H
#define __ASM_ARCH_PPMU_H __FILE__

#define NUMBER_OF_COUNTER	4

#define PPMU_CNTENS		0x10
#define PPMU_CNTENC		0x20
#define PPMU_INTENS		0x30
#define PPMU_INTENC		0x40

#define PPMU_CCNT		0x100
#define PPMU_PMCNT0		0x110
#define PPMU_PMCNT_OFFSET	0x10

#define PPMU_BEVT0SEL		0x1000
#define PPMU_BEVTSEL_OFFSET	0x100
#define PPMU_CNT_RESET		0x1800

#define DEVT0_SEL		0x1000
#define DEVT0_ID		0x1010
#define DEVT0_IDMSK		0x1014
#define DEVT_ID_OFFSET		0x100

#define DEFAULT_WEIGHT		1

#define MAX_CCNT		100

/* For Event Selection */
#define RD_DATA_COUNT		0x00000005
#define WR_DATA_COUNT		0x00000006
#define RDWR_DATA_COUNT		0x00000007

#define PMCNT_OFFSET(i)		(PPMU_PMCNT0 + (PPMU_PMCNT_OFFSET * i))

enum ppmu_counter {
	PPMU_PMNCNT0,
	PPMU_PMCCNT1,
	PPMU_PMNCNT2,
	PPMU_PMNCNT3,
	PPMU_PMNCNT_MAX,
};

enum ppmu_type {
	PPMU_MIF,
	PPMU_INT,
	PPMU_TYPE_END,
};

enum exynos_ppmu {
	PPMU_CPU,
	PPMU_DDR_C,
	PPMU_DDR_R1,
	PPMU_DDR_L,
	PPMU_RIGHT,
	PPMU_END,
};

extern unsigned long long ppmu_load[PPMU_END];

struct exynos_ppmu_hw {
	struct list_head node;
	void __iomem *hw_base;
	unsigned int ccnt;
	unsigned int event[NUMBER_OF_COUNTER];
	unsigned int weight;
	int usage;
	int id;
	struct device *dev;
	unsigned int count[NUMBER_OF_COUNTER];
};

void ppmu_init(struct exynos_ppmu_hw *ppmu, struct device *dev);
void ppmu_start(struct device *dev);
void ppmu_update(struct device *dev, int ch);
void ppmu_reset(struct device *dev);

extern struct exynos_ppmu_hw exynos_ppmu[];
#endif /* __ASM_ARCH_PPMU_H */

