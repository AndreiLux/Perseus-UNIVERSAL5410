/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_GPIO_EXYNOS_H
#define __ASM_ARCH_GPIO_EXYNOS_H __FILE__

#include <mach/gpio.h>

#if defined(CONFIG_MACH_UNIVERSAL5410)
#if defined(CONFIG_JA_LTE_KOR_REV00)
#include "gpio-exynos5410-lte-kor-rev00.h"
#elif defined(CONFIG_MACH_J_CHN_CTC)
#include "gpio-exynos5410-j-ctc-rev00.h"
#elif defined(CONFIG_MACH_J_CHN_CU)
#include "gpio-exynos5410-j-cu-rev00.h"
#elif defined(CONFIG_MACH_HA)
#include "gpio-ha-3g-rev00.h"
#elif defined(CONFIG_UNIVERSAL5410_3G_REV00)
#include "gpio-exynos5410-3g-rev00.h"
#elif defined(CONFIG_UNIVERSAL5410_LTE_REV00)
#if defined(CONFIG_TARGET_LOCALE_JPN)
#include "gpio-exynos5410-lte-jpn-rev00.h"
#else
#include "gpio-exynos5410-lte-rev00.h"
#endif
#elif defined(CONFIG_V1_LTE_REV00)
#include "gpio-v1-lte-rev00.h"
#elif defined(CONFIG_V1_3G_REV00)  || defined(CONFIG_V1_3G_REV03)
#include "gpio-v1-3g-rev00.h"
#elif defined(CONFIG_V1_WIFI_REV00)
#include "gpio-v1-wifi-rev00.h"
#endif
#endif

#ifdef CONFIG_SEC_PM
/* Exynos gpio configuration function point for sleep */
extern void (*exynos_set_sleep_gpio_table)(void);
extern void (*exynos_debug_show_gpio)(void);

/* To register gpio table */
extern int sec_gpio_init(void);

/* Sleep gpio configuration function */
extern void sec_config_sleep_gpio_table(void);
extern void sec_debug_show_gpio(void);

/* Init gpio configuration function */
extern void sec_config_gpio_table(void);
#endif

#endif /* __ASM_ARCH_GPIO_EXYNOS_H */
