/* linux/arch/arm/mach-exynos/include/mach/exynos-devfreq.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_DEVFREQ_H_
#define __EXYNOS_DEVFREQ_H_
enum devfreq_transition {
	MIF_DEVFREQ_PRECHANGE,
	MIF_DEVFREQ_POSTCHANGE,
	MIF_DEVFREQ_EN_MONITORING,
	MIF_DEVFREQ_DIS_MONITORING,
};

enum devfreq_media_type {
	TYPE_FIMC_LITE,
	TYPE_MIXER,
	TYPE_FIMD1,
};

#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
#define NUM_LAYERS_6	6
#define NUM_LAYERS_5	5
#define NUM_LAYERS_4	4
#define NUM_LAYERS_3	3
#define NUM_LAYERS_2	2
#define NUM_LAYERS_1	1
#define NUM_LAYERS_0	0
#endif

struct exynos_devfreq_platdata {
	unsigned int default_qos;
};

struct devfreq_info {
	unsigned int old;
	unsigned int new;
};

extern struct pm_qos_request exynos5_cpu_int_qos;
extern struct pm_qos_request exynos5_cpu_mif_qos;

extern int exynos5_mif_notify_transition(struct devfreq_info *info, unsigned int state);
extern int exynos5_mif_register_notifier(struct notifier_block *nb);
extern int exynos5_mif_unregister_notifier(struct notifier_block *nb);

extern int exynos5_mif_bpll_register_notifier(struct notifier_block *nb);
extern int exynos5_mif_bpll_unregister_notifier(struct notifier_block *nb);

extern spinlock_t int_div_lock;

#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
void exynos5_mif_nocp_resume(void);
void exynos5_mif_transition_disable(bool disable);
void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value);
#else
static inline
void exynos5_mif_nocp_resume(void)
{
	return;
}

static inline
void exynos5_mif_transition_disable(bool disable)
{
	return;
}

static inline
void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	return;
}
#endif

#endif
