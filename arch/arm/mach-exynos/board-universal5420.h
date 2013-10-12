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

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5420_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5420_H

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_charging_common.h>

extern sec_battery_platform_data_t sec_battery_pdata;
extern int current_cable_type;
#endif

extern unsigned int lpcharge;

void exynos5_universal5420_pmic_init(void);
void exynos5_universal5420_power_init(void);
void exynos5_universal5420_clock_init(void);
void exynos5_universal5420_mmc_init(void);
void exynos5_universal5420_usb_init(void);
#if defined(CONFIG_V1A) || defined(CONFIG_N1A)
void exynos5_vienna_battery_init(void);
#else
void exynos5_universal5420_battery_init(void);
#endif
void exynos5_universal5420_audio_init(void);
void exynos5_universal5420_input_init(void);
void exynos5_universal5420_display_init(void);
void exynos5_universal5420_media_init(void);
void exynos5_universal5420_mfd_init(void);
void exynos5_universal5420_sensor_init(void);
void exynos5_universal5420_nfc_init(void);
void exynos5_universal5420_vibrator_init(void);
void exynos5_universal5420_led_init(void);
#ifdef CONFIG_SAMSUNG_MHL_8240
void exynos5_universal5410_mhl_init(void);
#endif
void exynos5_universal5420_gpio_init(void);
void exynos5_universal5420_fpga_init(void);
void exynos5_universal5420_thermistor_init(void);
#ifdef CONFIG_W1
void exynos5_universal5420_cover_id_init(void);
#endif
#endif
