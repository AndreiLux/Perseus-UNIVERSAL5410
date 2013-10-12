/* drivers/gpu/t6xx/kbase/src/platform/mali_kbase_dvfs.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_dvfs.h
 * DVFS
 */

#ifndef _KBASE_DVFS_H_
#define _KBASE_DVFS_H_

/* Frequency that DVFS clock frequency decisions should be made */
#define KBASE_PM_DVFS_FREQUENCY                 100

#define MALI_DVFS_KEEP_STAY_CNT 10
#define MALI_DVFS_TIME_INTERVAL 5

#define MALI_DVFS_CURRENT_FREQ 0

#if SOC_NAME == 5420
#if defined(CONFIG_S5P_DP)
#define MALI_DVFS_START_FREQ 420
#else
#define MALI_DVFS_START_FREQ 177
#endif
#define MALI_DVFS_BL_CONFIG_FREQ 350
#define IS_ASV_ENABLED
#else
#define MALI_DVFS_START_FREQ 450
#define MALI_DVFS_BL_CONFIG_FREQ 533
#endif

#ifdef CONFIG_MALI_T6XX_DVFS
#define CONFIG_MALI_T6XX_FREQ_LOCK
typedef enum gpu_lock_type {
	TMU_LOCK = 0,
	SYSFS_LOCK,
	NUMBER_LOCK
} gpu_lock_type;
#ifdef CONFIG_CPU_FREQ
/* This define should be updated after ASV is enabled */
#ifdef IS_ASV_ENABLED
#define MALI_DVFS_ASV_ENABLE
#endif
#endif
#endif

#define DVFS_ASSERT(x) \
do { if (x) break; \
	printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n", __FILE__, __func__, __LINE__, #x); dump_stack(); \
} while (0)

extern unsigned int gpu_voltage_margin;
void kbase_set_power_margin(int);
#if defined(CONFIG_EXYNOS_THERMAL)
int kbase_tmu_hot_check_and_work(unsigned long event);
void kbase_tmu_normal_work(void);
#endif

struct regulator *kbase_platform_get_regulator(void);
int kbase_platform_regulator_init(void);
int kbase_platform_regulator_disable(void);
int kbase_platform_regulator_enable(void);
int kbase_platform_get_voltage(struct device *dev, int *vol);
int kbase_platform_set_voltage(struct device *dev, int vol);
void kbase_platform_dvfs_set_clock(kbase_device *kbdev, int freq);
int kbase_platform_dvfs_sprint_avs_table(char *buf, size_t buf_size);
int kbase_platform_dvfs_set(int enable);
void kbase_platform_dvfs_set_level(struct kbase_device *kbdev, int level);
int kbase_platform_dvfs_get_level(int freq);

#ifdef CONFIG_MALI_T6XX_DVFS
int kbase_platform_dvfs_init(struct kbase_device *dev);
void kbase_platform_dvfs_term(void);
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation);
int kbase_platform_dvfs_get_enable_status(void);
int kbase_platform_dvfs_enable(bool enable, int freq);
int kbase_platform_dvfs_get_utilisation(void);
#endif

#ifdef CONFIG_MALI_DEVFREQ
int mali_devfreq_remove(void);
int mali_devfreq_add(struct kbase_device *kbdev);
#endif

int mali_get_dvfs_step(void);
int mali_get_dvfs_clock(int level);
int mali_get_dvfs_table(char *buf, size_t buf_size);
int mali_get_dvfs_current_level(void);
int mali_get_dvfs_upper_locked_freq(void);
int mali_get_dvfs_under_locked_freq(void);
#ifdef CONFIG_SOC_EXYNOS5420
int mali_dvfs_freq_max_lock(int level, gpu_lock_type user_lock);
void mali_dvfs_freq_max_unlock(gpu_lock_type user_lock);
int mali_dvfs_freq_min_lock(int level, gpu_lock_type user_lock);
void mali_dvfs_freq_min_unlock(gpu_lock_type user_lock);
#else
int mali_dvfs_freq_lock(int level);
void mali_dvfs_freq_unlock(void);
int mali_dvfs_freq_under_lock(int level);
void mali_dvfs_freq_under_unlock(void);
int mali_dvfs_freq_min_lock(int level);
void mali_dvfs_freq_min_unlock(void);
#endif

int mali_get_dvfs_max_locked_freq(void);
int mali_get_dvfs_min_locked_freq(void);

ssize_t show_time_in_state(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t set_time_in_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

#endif /* _KBASE_DVFS_H_ */
