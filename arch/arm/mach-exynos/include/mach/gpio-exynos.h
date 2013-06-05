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
#if defined(CONFIG_UNIVERSAL5410_3G_REV00)
#include "gpio-exynos5410-3g-rev00.h"
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
