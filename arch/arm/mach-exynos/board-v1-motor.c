/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/i2c-gpio.h>
#include <linux/isa1400_vibrator.h>

#define MOTOR_PWM_PERIOD		485
#define MOTOR_MAX_TIMEOUT	10000

struct pwm_device	*motor_pwm;
static u8 motor_pwm_id;
extern unsigned int system_rev;

static u8 isa1400_init[] = {
	ISA1400_REG_INIT, 0,
	ISA1400_REG_AMPGAIN1, 0x57,
	ISA1400_REG_AMPGAIN2, 0x57,
	ISA1400_REG_OVDRTYP, 0xca,
	ISA1400_REG_AMPSWTCH, 0x1a,
	ISA1400_REG_SYSCLK, 0x21,
	ISA1400_REG_GAIN, 0x00,
	ISA1400_REG_GAIN2, 0x00,
	ISA1400_REG_GAIN3, 0x00,
	ISA1400_REG_GAIN4, 0x00,
	ISA1400_REG_FREQ1M, 0x0c,
	ISA1400_REG_FREQ1L, 0xd6,
	ISA1400_REG_FREQ2M, 0x0c,
	ISA1400_REG_FREQ2L, 0xd6,
	ISA1400_REG_FREQ3M, 0x0c,
	ISA1400_REG_FREQ3L, 0xd6,
	ISA1400_REG_FREQ4M, 0x0c,
	ISA1400_REG_FREQ4L, 0xd6,
	ISA1400_REG_HPTEN, 0x0f,
	ISA1400_REG_CHMODE, 0x00,
	ISA1400_REG_WAVESEL, 0x00,
};

static u8 isa1400_start[] = {
	ISA1400_REG_START, 0,
	ISA1400_REG_GAIN, 0x7f,
	ISA1400_REG_GAIN2, 0x7f,
	ISA1400_REG_GAIN3, 0x7f,
	ISA1400_REG_GAIN4, 0x7f,
};

static u8 isa1400_stop[] = {
	ISA1400_REG_STOP, 0,
	ISA1400_REG_GAIN, 0x00,
	ISA1400_REG_GAIN2, 0x00,
	ISA1400_REG_GAIN3, 0x00,
	ISA1400_REG_GAIN4, 0x00,
	ISA1400_REG_HPTEN, 0x00,
};

static const u8 *isa1400_reg_data[] = {
	isa1400_init,
	isa1400_start,
	isa1400_stop,
};

static int isa1400_vdd_en(bool en)
{
	int ret = 0;

	pr_info("[VIB] %s %s\n",
		__func__, en ? "on" : "off");

	ret = gpio_direction_output(GPIO_MOTOR_EN, en);

	if (en)
		msleep(20);

	return ret;
}

static int isa1400_clk_en(bool en)
{
	if (!motor_pwm) {
		motor_pwm =
			pwm_request(motor_pwm_id, "vibrator");
		if (IS_ERR(motor_pwm))
			pr_err("[VIB] Failed to request pwm.\n");
		else
			pwm_config(motor_pwm,
				MOTOR_PWM_PERIOD / 2,
				MOTOR_PWM_PERIOD);
	}

	if (en)
		return pwm_enable(motor_pwm);
	else
		pwm_disable(motor_pwm);

	return 0;
}

static struct i2c_gpio_platform_data gpio_i2c_data20 = {
	.sda_pin = GPIO_MOTOR_SDA,
	.scl_pin = GPIO_MOTOR_SCL,
};

struct platform_device s3c_device_i2c20 = {
	.name = "i2c-gpio",
	.id = 20,
	.dev.platform_data = &gpio_i2c_data20,
};

const u8 actuators[] = {CH1, CH3,};

static struct isa1400_vibrator_platform_data isa1400_vibrator_pdata = {
	.gpio_en = isa1400_vdd_en,
	.clk_en = isa1400_clk_en,
	.max_timeout = MOTOR_MAX_TIMEOUT,
	.reg_data = isa1400_reg_data,
	.actuator = actuators,
	.actuactor_num = ARRAY_SIZE(actuators),
};

static struct i2c_board_info i2c_devs20_emul[] __initdata = {
	{
		I2C_BOARD_INFO("isa1400_vibrator",  (0x90 >> 1)),
		.platform_data = &isa1400_vibrator_pdata,
	},
};

void vienna_motor_init(void)
{
	u32 gpio, gpio_motor_pwm;

	pr_info("[VIB] system_rev %d\n", system_rev);

	gpio = GPIO_MOTOR_EN;
	gpio_request(gpio, "MOTOR_EN");
	gpio_direction_output(gpio, 1);
	gpio_export(gpio, 0);

	if (system_rev >= 0x3) {
		motor_pwm_id = 1;
		gpio_motor_pwm = EXYNOS5410_GPB2(1);
		gpio_i2c_data20.sda_pin = EXYNOS5410_GPB0(0);
		gpio_i2c_data20.scl_pin = EXYNOS5410_GPB0(1);
	} else {
		motor_pwm_id = 0;
		gpio_motor_pwm = EXYNOS5410_GPB2(0);
	}

	gpio_request(gpio_motor_pwm, "motor_pwm");
	s3c_gpio_cfgpin(gpio_motor_pwm, S3C_GPIO_SFN(0x2));
	platform_device_register(&s3c_device_timer[motor_pwm_id]);

	isa1400_init[1] = sizeof(isa1400_init);
	isa1400_start[1] = sizeof(isa1400_start);
	isa1400_stop[1] = sizeof(isa1400_stop);

	i2c_register_board_info(20, i2c_devs20_emul,
				ARRAY_SIZE(i2c_devs20_emul));

	platform_device_register(&s3c_device_i2c20);
}
