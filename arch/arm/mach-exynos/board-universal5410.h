/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5410_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5410_H
#include <linux/i2c.h>

void exynos5_universal5410_clock_init(void);
void exynos5_universal5410_mmc_init(void);
void exynos5_universal5410_power_init(void);
void exynos5_universal5410_usb_init(void);
void exynos5_universal5410_audio_init(void);
void exynos5_universal5410_display_init(void);
void exynos5_universal5410_input_init(void);
void exynos5_universal5410_media_init(void);

extern unsigned int universal5410_rev(void);

#define PMIC_I2C_DEVS_MAX 1
extern struct i2c_board_info hs_i2c_devs0[PMIC_I2C_DEVS_MAX];
#endif
