/*
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/platform_data/exynos_thermal.h>

#include <asm/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/devfreq.h>
#ifdef CONFIG_SEC_PM
#include <mach/gpio-exynos.h>
#endif
#include <mach/tmu.h>

#include "board-universal5410.h"

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
	.id	= -1,
};
#endif

#ifdef CONFIG_PM_DEVFREQ
static struct platform_device exynos5_mif_devfreq = {
	.name	= "exynos5-busfreq-mif",
	.id	= -1,
};

static struct platform_device exynos5_int_devfreq = {
	.name	= "exynos5-busfreq-int",
	.id	= -1,
};

static struct exynos_devfreq_platdata universal5410_qos_mif_pd __initdata = {
	.default_qos = 200000,
};

static struct exynos_devfreq_platdata universal5410_qos_int_pd __initdata = {
	.default_qos = 50000,
};
#endif

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
static struct exynos_tmu_platform_data exynos5_tmu_data = {
	.trigger_levels[0] = 90,
	.trigger_levels[1] = 95,
	.trigger_levels[2] = 110,
	.trigger_levels[3] = 115,
	.boost_trigger_levels[0] = 100,
	.boost_trigger_levels[1] = 105,
	.boost_trigger_levels[2] = 110,
	.boost_trigger_levels[3] = 115,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 1,
	.gain = 5,
	.reference_voltage = 16,
	.noise_cancel_mode = 4,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 1400 * 1000,
		.temp_level = 90,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1000 * 1000,
		.temp_level = 95,
	},
	.freq_tab[2] = {
		.freq_clip_max = 600 * 1000,
		.temp_level = 100,
	},
	.boost_freq_tab[0] = {
		.freq_clip_max = 1400 * 1000,
		.temp_level = 100,
	},
	.boost_freq_tab[1] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 105,
	},
	.boost_freq_tab[2] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 105,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 2,
	.freq_tab_count = 3,
	.type = SOC_ARCH_EXYNOS5,
};
#else
static struct exynos_tmu_platform_data exynos5_tmu_data = {
	.trigger_levels[0] = 90,
	.trigger_levels[1] = 95,
	.trigger_levels[2] = 120,
	.boost_trigger_levels[0] = 100,
	.boost_trigger_levels[1] = 105,
	.boost_trigger_levels[2] = 120,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.gain = 5,
	.reference_voltage = 16,
	.noise_cancel_mode = 4,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 1400 * 1000,
		.temp_level = 90,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1000 * 1000,
		.temp_level = 95,
	},
	.freq_tab[2] = {
		.freq_clip_max = 600 * 1000,
		.temp_level = 100,
	},
	.freq_tab[3] = {
		.freq_clip_max = 200 * 1000,
		.temp_level = 120,
	},
	.boost_temps[0] = 100,
	.boost_temps[1] = 105,
	.boost_temps[2] = 110,
	.boost_temps[3] = 120,
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 2,
	.size[THERMAL_TRIP_HOT] = 1,
	.freq_tab_count = 4,
	.type = SOC_ARCH_EXYNOS5,
};
#endif

static struct platform_device *universal5410_power_devices[] __initdata = {
	&exynos5_device_hs_i2c3,
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
#ifdef CONFIG_SEC_THERMISTOR
	&sec_device_thermistor,
#endif
#ifdef CONFIG_PM_DEVFREQ
	&exynos5_mif_devfreq,
	&exynos5_int_devfreq,
#endif
	&exynos5410_device_tmu,
};

struct exynos5_platform_i2c hs_i2c3_data __initdata = {
	.bus_number = 7,
	.operation_mode = 0,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

void __init exynos5_universal5410_power_init(void)
{
	exynos5_hs_i2c3_set_platdata(&hs_i2c3_data);
	i2c_register_board_info(7, hs_i2c_devs0, ARRAY_SIZE(hs_i2c_devs0));
	
#ifdef CONFIG_PM_DEVFREQ
	s3c_set_platdata(&universal5410_qos_mif_pd, sizeof(struct exynos_devfreq_platdata),
			&exynos5_mif_devfreq);

	s3c_set_platdata(&universal5410_qos_int_pd, sizeof(struct exynos_devfreq_platdata),
			&exynos5_int_devfreq);
#endif
	s3c_set_platdata(&exynos5_tmu_data, sizeof(struct exynos_tmu_platform_data),
			&exynos5410_device_tmu);

	platform_add_devices(universal5410_power_devices,
			ARRAY_SIZE(universal5410_power_devices));

#ifdef CONFIG_SEC_PM
	sec_gpio_init();
	sec_config_gpio_table();
	exynos_set_sleep_gpio_table = sec_config_sleep_gpio_table;
	exynos_debug_show_gpio = sec_debug_show_gpio;
#endif
}
