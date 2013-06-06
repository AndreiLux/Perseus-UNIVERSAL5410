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

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_charging_common.h>
extern sec_battery_platform_data_t sec_battery_pdata;
extern int current_cable_type;
#endif

void exynos5_universal5410_clock_init(void);
void exynos5_universal5410_mmc_init(void);
void exynos5_universal5410_power_init(void);
void exynos5_universal5410_battery_init(void);
void exynos5_universal5410_usb_init(void);
void exynos5_universal5410_audio_init(void);
void exynos5_universal5410_2mic_init(void);
void exynos5_universal5410_display_init(void);
void exynos5_universal5410_input_init(void);
void exynos5_universal5410_sensor_init(void);
void exynos5_universal5410_nfc_init(void);
void exynos5_universal5410_media_init(void);
void exynos5_universal5410_led_init(void);
void exynos5_universal5410_fpga_init(void);
void exynos5_universal5410_vibrator_init(void);
void exynos5_universal5410_mhl_init(void);
void exynos5_universal5410_mfd_init(void);
#ifdef CONFIG_EXYNOS_C2C
void exynos5_universal5410_c2c_init(void);
#endif

extern unsigned int system_rev;
extern unsigned int universal5410_rev(void);

extern unsigned int lcdtype;

#define PMIC_I2C_DEVS_MAX 1
extern struct i2c_board_info hs_i2c_devs0[PMIC_I2C_DEVS_MAX];

extern unsigned int lpcharge;
#endif

