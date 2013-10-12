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
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/i2c-gpio.h>
#include <linux/notifier.h>
#include <linux/haptic_isa1200.h>
#include <linux/haptic_isa1400.h>

#include <plat/iic.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>

#include <asm/system_info.h>

#define MOTOR_MAX_TIMEOUT	10000
#define MOTOR_PWM_ID			1

#define ISA1200_PWM_PERIOD		38675
#define ISA1400_PWM_PERIOD		485

#define GPIO_MOTOR_PWM		EXYNOS5420_GPB2(1)

static struct pwm_device *motor_pwm;
static int motor_pwm_period;

static void motor_pwm_init(void)
{
	motor_pwm =
		pwm_request(MOTOR_PWM_ID, "vibrator");
	if (IS_ERR(motor_pwm))
		pr_err("[VIB] Failed to request pwm.\n");
}

static int motor_vdd_en(bool en)
{
	return gpio_direction_output(GPIO_MOTOR_EN, en);
}

static int motor_pwm_en(bool en)
{

	if (IS_ERR(motor_pwm)) {
		pr_err("[VIB] pwm is not ready.\n");
		return -ENXIO;
	}

	if (0x3 != system_rev)
		pwm_config(motor_pwm,
			motor_pwm_period >> 1,
			motor_pwm_period);

	if (en)
		return pwm_enable(motor_pwm);
	else
		pwm_disable(motor_pwm);

	return 0;
}

static int motor_pwm_config(int duty)
{
	u32 duty_ns = (u32)(duty * motor_pwm_period)
		/ 1000;

	if (IS_ERR(motor_pwm)) {
		pr_err("[VIB] pwm is not ready.\n");
		return -ENXIO;
	}

	return pwm_config(motor_pwm,
		(int)duty_ns, motor_pwm_period);
}

static u8 isa1200_init[] = {
	ISA1200_REG_INIT, 0,
	ISA1200_CTRL0, CTL0_DIVIDER128 | CTL0_PWM_INPUT,
	ISA1200_CTRL1, CTL1_DEFAULT,
	ISA1200_CTRL2, 0x00,
	ISA1200_CTRL3, 0x23,
	ISA1200_CTRL4, 0x00,
	ISA1200_DUTY, (0x74 >> 1),
	ISA1200_PERIOD, 0x74,
};

static u8 isa1200_start[] = {
	ISA1200_REG_START, 0,
	ISA1200_CTRL0, CTL0_DIVIDER128 | CTL0_PWM_INPUT | CTL0_NORMAL_OP,
};

static u8 isa1200_stop[] = {
	ISA1200_REG_STOP, 0,
	ISA1200_CTRL0, CTL0_DIVIDER128 | CTL0_PWM_INPUT,
};

static const u8 *isa120_reg_data[] = {
	isa1200_init,
	isa1200_start,
	isa1200_stop,
};

static struct isa1200_pdata isa1200_vibrator_pdata = {
	.gpio_en = motor_vdd_en,
	.pwm_en = motor_pwm_en,
	.pwm_cfg = motor_pwm_config,
	.pwm_init = motor_pwm_init,
	.max_timeout = MOTOR_MAX_TIMEOUT,
	.reg_data = isa120_reg_data,
};

static struct i2c_board_info i2c_devs_isa1200[] __initdata = {
	{
		I2C_BOARD_INFO(ISA1200_DEVICE_NAME,  0x48),
		.platform_data = &isa1200_vibrator_pdata,
	},
};

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

const u8 actuators[] = {CH1, CH3,};

static struct isa1400_pdata isa1400_vibrator_pdata = {
	.gpio_en = motor_vdd_en,
	.clk_en = motor_pwm_en,
	.pwm_init = motor_pwm_init,
	.max_timeout = MOTOR_MAX_TIMEOUT,
	.reg_data = isa1400_reg_data,
	.actuator = actuators,
	.actuactor_num = ARRAY_SIZE(actuators),
};

static struct i2c_board_info i2c_devs_isa1400[] __initdata = {
	{
		I2C_BOARD_INFO(ISA1400_DEVICE_NAME,  (0x90 >> 1)),
		.platform_data = &isa1400_vibrator_pdata,
	},
};

static struct i2c_gpio_platform_data gpio_i2c_data20 = {
	.sda_pin = GPIO_MOTOR_SDA,
	.scl_pin = GPIO_MOTOR_SCL,
};

struct platform_device s3c_device_i2c20 = {
	.name = "i2c-gpio",
	.id = 20,
	.dev.platform_data = &gpio_i2c_data20,
};

void vienna_motor_init(void)
{
	pr_info("[VIB] system_rev %d\n", system_rev);

	gpio_request(GPIO_MOTOR_EN, "MOTOR_EN");
	gpio_direction_output(GPIO_MOTOR_EN, 0);
	gpio_export(GPIO_MOTOR_EN, 0);

	gpio_request(GPIO_MOTOR_PWM, "motor_pwm");
	s3c_gpio_cfgpin(GPIO_MOTOR_PWM, S3C_GPIO_SFN(0x2));
	platform_device_register(&s3c_device_timer[MOTOR_PWM_ID]);

	if (0x3 != system_rev) {
		isa1400_init[1] = sizeof(isa1400_init);
		isa1400_start[1] = sizeof(isa1400_start);
		isa1400_stop[1] = sizeof(isa1400_stop);
		motor_pwm_period = ISA1400_PWM_PERIOD;
		i2c_register_board_info(20, i2c_devs_isa1400,
			ARRAY_SIZE(i2c_devs_isa1400));
	} else {
		isa1200_init[1] = sizeof(isa1200_init);
		isa1200_start[1] = sizeof(isa1200_start);
		isa1200_stop[1] = sizeof(isa1200_stop);
		motor_pwm_period = ISA1200_PWM_PERIOD;
		i2c_register_board_info(20, i2c_devs_isa1200,
			ARRAY_SIZE(i2c_devs_isa1200));
	}

	platform_device_register(&s3c_device_i2c20);
}
