/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_data/es325.h>

#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/devs.h>

#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/exynos5-audio.h>

#include "board-universal5410.h"


static struct es325_veq_parm veq_parm_table[] = {
	{ 0,  0},
	{ 2,  6},
	{ 2,  9},
	{ 2, 12},
	{ 2,  9},
	{ 2,  6},
	{19,  3},
};

static struct es325_platform_data es325_pdata = {
	.gpio_wakeup = EXYNOS5410_GPJ3(7),
	.gpio_reset = EXYNOS5410_GPJ2(3),
	.clk_enable = exynos5_audio_set_mclk,
	.pass[0] = {1, 3},
	.pass[1] = {2, 4},
	.passthrough_src = 1,
	.passthrough_dst = 3,
	.use_sleep = true,
	.veq_parm_table = veq_parm_table,
	.veq_parm_table_size = ARRAY_SIZE(veq_parm_table),
};

static struct platform_device *universal5410_2mic_devices[] __initdata = {
	&exynos5_device_hs_i2c0,
};

struct exynos5_platform_i2c hs_i2c4_data __initdata = {
	.bus_number = 4,
	.operation_mode = HSI2C_INTERRUPT,
	.speed_mode = HSI2C_HIGH_SPD,
	.fast_speed = 400000,
	.high_speed = 800000,
	.cfg_gpio = NULL,
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("audience_es325", 0x3e),
		.platform_data = &es325_pdata,
	},
};

static void universal5410_2mic_gpio_init(void)
{
	s3c_gpio_cfgpin(EXYNOS5410_GPA1(5), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS5410_GPA1(5), S3C_GPIO_PULL_NONE);
}

void __init exynos5_universal5410_2mic_init(void)
{
	int ret;
	universal5410_2mic_gpio_init();

	if (system_rev < 0x3)
		es325_pdata.use_sleep = false;

	exynos5_hs_i2c0_set_platdata(&hs_i2c4_data);
	ret = i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));

	platform_add_devices(universal5410_2mic_devices,
			ARRAY_SIZE(universal5410_2mic_devices));
}
