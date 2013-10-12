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

#if defined(CONFIG_MACH_UNIVERSAL5420)
#if defined(CONFIG_V1A)
#include "gpio-v1a-3g-rev00.h"
#elif defined(CONFIG_N1A)
#include "gpio-n1a-rev00.h"
#else
#include "gpio-ha-3g-rev00.h"
#endif
#endif

extern void (*exynos_config_sleep_gpio)(void);

#endif /* __ASM_ARCH_GPIO_EXYNOS_H */
