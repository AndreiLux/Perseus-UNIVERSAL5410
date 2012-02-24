/* linux/arch/arm/mach-exynos/include/mach/busfreq_exynos5.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 - BUSFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_BUSFREQ_H
#define __ASM_ARCH_BUSFREQ_H __FILE__

#include <linux/notifier.h>

#include <mach/ppmu.h>

#define LOAD_HISTORY_SIZE	5

enum busfreq_level_idx {
	LV_0,
	LV_1,
	LV_2,
	LV_3,
	LV_INT_END,
	LV_MIF_END = LV_3,
};

struct opp;
struct device;
struct busfreq_table;

struct busfreq_data {
	bool use;
	int index;
	unsigned int sampling_rate;
	struct delayed_work worker;

	struct device *dev[PPMU_TYPE_END];
	unsigned long curr_freq[PPMU_TYPE_END];
	unsigned long max_freq[PPMU_TYPE_END];
	unsigned long min_freq[PPMU_TYPE_END];
	struct regulator *vdd_reg[PPMU_TYPE_END];
	struct busfreq_table *table[PPMU_TYPE_END];
	unsigned int load_history[PPMU_END][LOAD_HISTORY_SIZE];

	struct notifier_block exynos_buspm_notifier;
	struct notifier_block exynos_reboot_notifier;
	struct notifier_block exynos_request_notifier;
	int (*init)	(struct device *dev, struct busfreq_data *data);
	void (*monitor) (struct busfreq_data *data, struct opp **mif_opp,
			struct opp **int_opp);
	void (*target)	(enum ppmu_type type, int index);
	unsigned int (*get_int_volt) (unsigned long freq);
	int (*get_table_index) (unsigned long freq, enum ppmu_type type);
	void (*busfreq_suspend) (void);
	void (*busfreq_resume) (void);
};

struct busfreq_table {
	unsigned int idx;
	unsigned int mem_clk;
	unsigned int volt;
	unsigned int clk_topdiv;
	unsigned int clk_dmc0div;
	unsigned int clk_dmc1div;
};

void exynos_request_apply(unsigned long freq);
unsigned long step_down(struct busfreq_data *data, enum ppmu_type type, int step);

int exynos5250_init(struct device *dev, struct busfreq_data *data);
#endif /* __ASM_ARCH_BUSFREQ_H */
