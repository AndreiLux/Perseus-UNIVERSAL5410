/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>

#include <plat/gpio-cfg.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-exynos.h>

#include "board-universal5410.h"

#ifdef CONFIG_IR_REMOCON_MC96
#include <linux/ir_remote_con_mc96.h>
#endif

static void irda_wake_en(bool onoff)
{
	gpio_direction_output(GPIO_IRDA_WAKE, onoff);
#if defined(CONFIG_V1_3G_REV00)  || defined(CONFIG_V1_3G_REV03)
	gpio_set_value(GPIO_IR_LED_EN, onoff);
#endif
	printk(KERN_ERR "%s: %d\n", __func__, onoff);
}

static void irda_device_init(void)
{
	int ret;

	printk("%s called!\n", __func__);

	ret = gpio_request(GPIO_IRDA_WAKE, "irda_wake");
	if (ret) {
		printk(KERN_ERR "%s: gpio_request fail[%d], ret = %d\n",
				__func__, GPIO_IRDA_WAKE, ret);
		return;
	}
	gpio_direction_output(GPIO_IRDA_WAKE, 0);
#if defined(CONFIG_V1_3G_REV00)  || defined(CONFIG_V1_3G_REV03)
	ret = gpio_request(GPIO_IR_LED_EN, "ir_led_en");
	if (ret) {
		printk(KERN_ERR "%s: gpio_request fail[%d], ret = %d\n",
				__func__, GPIO_IR_LED_EN, ret);
		return;
	}
	gpio_direction_output(GPIO_IR_LED_EN, 0);
#endif
	s3c_gpio_cfgpin(GPIO_IRDA_IRQ, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_IRDA_IRQ, S3C_GPIO_PULL_UP);
}

static int vled_ic_onoff;

static void irda_vdd_onoff(bool onoff)
{
	static struct regulator *vled_ic;

	if (onoff) {
		vled_ic = regulator_get(NULL, "vcc_1.9v_irda");
		if (IS_ERR(vled_ic)) {
			pr_err("could not get regulator vcc_1.9v_irda\n");
			return;
		}
		regulator_enable(vled_ic);
		vled_ic_onoff = 1;
	} else if (vled_ic_onoff == 1) {
		regulator_force_disable(vled_ic);
		regulator_put(vled_ic);
		vled_ic_onoff = 0;
	}
}

static struct i2c_gpio_platform_data gpio_i2c_data22 = {
	.sda_pin = GPIO_IRDA_SDA,
	.scl_pin = GPIO_IRDA_SCL,
	.udelay = 2,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

struct platform_device s3c_device_i2c22 = {
	.name = "i2c-gpio",
	.id = 22,
	.dev.platform_data = &gpio_i2c_data22,
};

static struct mc96_platform_data mc96_pdata = {
	.ir_wake_en = irda_wake_en,
	.ir_vdd_onoff = irda_vdd_onoff,
};

static struct i2c_board_info i2c_devs22_emul[] __initdata = {
	{
		I2C_BOARD_INFO("mc96", (0XA0 >> 1)),
		.platform_data = &mc96_pdata,
	},
};

static struct platform_device *v1_devices[] __initdata = {
	&s3c_device_i2c22,
};

void __init v1_irda_init(void)
{
	printk("[%s] initialization start\n", __func__);
	printk("system_rev %d\n", system_rev);

	if (system_rev >= 0x03) {
		printk("I2C changed\n");
		gpio_i2c_data22.sda_pin = EXYNOS5410_GPG1(0);
		gpio_i2c_data22.scl_pin = EXYNOS5410_GPG1(1);
	}

	printk("[%s] IRDA SCL : %d\n", __func__, gpio_i2c_data22.scl_pin);
	printk("[%s] IRDA SDA : %d\n", __func__, gpio_i2c_data22.sda_pin);

	i2c_register_board_info(22, i2c_devs22_emul,
				ARRAY_SIZE(i2c_devs22_emul));

	platform_add_devices(v1_devices,
			ARRAY_SIZE(v1_devices));

	irda_device_init();
	printk("[%s] initialization end\n", __func__);
};
