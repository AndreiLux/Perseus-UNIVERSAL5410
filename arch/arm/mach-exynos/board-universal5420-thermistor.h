/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5420_THERM_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5420_THERM_H

#if defined(CONFIG_HA)
extern void board_h_thermistor_init(void);
#elif defined(CONFIG_V1A)
extern void board_v1_thermistor_init(void);
#elif defined(CONFIG_N1A)
extern void board_n1_thermistor_init(void);
#endif

void __init exynos5_universal5420_thermistor_init(void)
{
#if defined(CONFIG_HA)
	board_h_thermistor_init();
#elif defined(CONFIG_V1A)
	board_v1_thermistor_init();
#elif defined(CONFIG_N1A)
	board_n1_thermistor_init();
#endif
}

#endif /* __MACH_EXYNOS_BOARD_UNIVERSAL5420_THERM_H */
