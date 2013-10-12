/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <plat/devs.h>
#include <linux/platform_data/exynos_thermal.h>
#include <mach/tmu.h>
#include <mach/devfreq.h>

#ifdef CONFIG_BATTERY_SAMSUNG
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
	.id	= -1,
};
#endif

static struct exynos_tmu_platform_data exynos5_tmu_data = {
	.trigger_levels[0] = 90,
	.trigger_levels[1] = 95,
	.trigger_levels[2] = 110,
	.trigger_levels[3] = 115,
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
		.freq_clip_max = 1700 * 1000,
		.temp_level = 90,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 95,
	},
	.freq_tab[2] = {
		.freq_clip_max = 600 * 1000,
		.temp_level = 100,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 2,
	.freq_tab_count = 3,
	.type = SOC_ARCH_EXYNOS5,
};

#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
static struct platform_device exynos5_mif_devfreq = {
	.name	= "exynos5-busfreq-mif",
	.id	= -1,
};

static struct exynos_devfreq_platdata smdk5420_qos_mif_pd __initdata = {
#if defined(CONFIG_S5P_DP)
	.default_qos = 160000,
#else
	.default_qos = 133000,
#endif
};

static struct platform_device exynos5_int_devfreq = {
	.name	= "exynos5-busfreq-int",
	.id	= -1,
};

static struct exynos_devfreq_platdata smdk5420_qos_int_pd __initdata = {
#if defined(CONFIG_S5P_DP)
	.default_qos = 133000,
#else
	.default_qos = 83000,
#endif
};
#endif

static struct platform_device *universal5420_power_devices[] __initdata = {
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos5420_device_tmu,
#endif
#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
	&exynos5_mif_devfreq,
	&exynos5_int_devfreq,
#endif
};

void __init exynos5_universal5420_power_init(void)
{
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos5_tmu_data);
#endif

#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
	s3c_set_platdata(&smdk5420_qos_mif_pd, sizeof(struct exynos_devfreq_platdata),
			&exynos5_mif_devfreq);
	s3c_set_platdata(&smdk5420_qos_int_pd, sizeof(struct exynos_devfreq_platdata),
			&exynos5_int_devfreq);
#endif

	platform_add_devices(universal5420_power_devices,
			ARRAY_SIZE(universal5420_power_devices));
}
