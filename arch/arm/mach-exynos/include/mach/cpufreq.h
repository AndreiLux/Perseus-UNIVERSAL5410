/* linux/arch/arm/mach-exynos/include/mach/cpufreq.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPUFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

enum cpufreq_level_index {
	L0, L1, L2, L3, L4,
	L5, L6, L7, L8, L9,
	L10, L11, L12, L13, L14,
	L15, L16, L17, L18, L19,
	L20,
};

enum cpufreq_lock_ID {
	DVFS_LOCK_ID_G2D,	/* G2D */
	DVFS_LOCK_ID_TV,	/* TV */
	DVFS_LOCK_ID_MFC,	/* MFC */
	DVFS_LOCK_ID_USB,	/* USB */
	DVFS_LOCK_ID_CAM,	/* CAM */
	DVFS_LOCK_ID_PM,	/* PM */
	DVFS_LOCK_ID_USER,	/* USER */
	DVFS_LOCK_ID_TMU,	/* TMU */
	DVFS_LOCK_ID_LPA,	/* LPA */
	DVFS_LOCK_ID_DRM,	/* DRM */
	DVFS_LOCK_ID_END,
};

int exynos_cpufreq_get_level(unsigned int freq,
			unsigned int *level);
int exynos_cpufreq_lock(unsigned int nId,
			enum cpufreq_level_index cpufreq_level);
void exynos_cpufreq_lock_free(unsigned int nId);

int exynos_cpufreq_upper_limit(unsigned int nId,
			enum cpufreq_level_index cpufreq_level);
void exynos_cpufreq_upper_limit_free(unsigned int nId);

struct exynos_dvfs_info {
	unsigned long	mpll_freq_khz;
	unsigned int	pll_safe_idx;
	unsigned int	pm_lock_idx;
	unsigned int	max_support_idx;
	unsigned int	min_support_idx;
	struct clk	*cpu_clk;
	unsigned int	*volt_table;
	struct cpufreq_frequency_table	*freq_table;
	void (*set_freq)(unsigned int, unsigned int);
	bool (*need_apll_change)(unsigned int, unsigned int);
};

extern int exynos4210_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos4x12_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos5250_cpufreq_init(struct exynos_dvfs_info *);
