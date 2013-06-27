/* linux/arch/arm/mach-exynos/board-universal_5410-led.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <mach/gpio-exynos.h>

#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#include <linux/regulator/machine.h>
#ifdef CONFIG_LEDS_MAX77803
#include <linux/leds-max77803.h>
#endif
#endif

#ifdef CONFIG_MFD_MAX77693
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/regulator/machine.h>
#ifdef CONFIG_LEDS_MAX77693
#include <linux/leds-max77693.h>
#endif
#endif

#include "board-universal5410.h"

#ifdef CONFIG_MFD_MAX77803

static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

static struct regulator_consumer_supply charger_supply[] = {
	REGULATOR_SUPPLY("vinchg1", "charger-manager.0"),
};

static struct regulator_init_data safeout1_init_data = {
	.constraints	= {
		.name		= "safeout1 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout1_supply),
	.consumer_supplies	= safeout1_supply,
};

static struct regulator_init_data safeout2_init_data = {
	.constraints	= {
		.name		= "safeout2 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 0,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout2_supply),
	.consumer_supplies	= safeout2_supply,
};

static struct regulator_init_data charger_init_data = {
	.constraints	= {
		.name		= "CHARGER",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
		REGULATOR_CHANGE_CURRENT,
		.boot_on	= 1,
		.min_uA		= 60000,
		.max_uA		= 2580000,
	},
	.num_consumer_supplies	= ARRAY_SIZE(charger_supply),
	.consumer_supplies	= charger_supply,
};

struct max77803_regulator_data max77803_regulators[] = {
	{MAX77803_ESAFEOUT1, &safeout1_init_data,},
	{MAX77803_ESAFEOUT2, &safeout2_init_data,},
	{MAX77803_CHARGER, &charger_init_data,},
};

static bool is_muic_default_uart_path_cp(void)
{
	return false;
}

#ifdef CONFIG_LEDS_MAX77803
static struct max77803_led_platform_data max77803_led_pdata = {

	.num_leds = 2,

	.leds[0].name = "leds-sec1",
	.leds[0].id = MAX77803_FLASH_LED_1,
	.leds[0].timer = MAX77803_FLASH_TIME_187P5MS,
	.leds[0].timer_mode = MAX77803_TIMER_MODE_MAX_TIMER,
	.leds[0].cntrl_mode = MAX77803_LED_CTRL_BY_FLASHSTB,
	.leds[0].brightness = 0x3D,
    //.leds[0].brightness = 0x1E,

	.leds[1].name = "torch-sec1",
	.leds[1].id = MAX77803_TORCH_LED_1,
	.leds[1].cntrl_mode = MAX77803_LED_CTRL_BY_FLASHSTB,
	.leds[1].brightness = 0x06,
};

#endif
#ifdef CONFIG_VIBETONZ
static struct max77803_haptic_platform_data max77803_haptic_pdata = {
	.max_timeout = 10000,
#if defined(CONFIG_MACH_JA_KOR_SKT) \
	   || defined(CONFIG_MACH_JA_KOR_KT) || defined(CONFIG_MACH_JA_KOR_LGT)
	.duty = 34157,
#else
	.duty = 35804,
#endif
	.period = 38054,
	.reg2 = MOTOR_LRA | EXT_PWM | DIVIDER_128,
	.init_hw = NULL,
	.motor_en = NULL,
	.pwm_id = 0,
	.regulator_name = "vcc_3.0v_motor",
};
#endif

struct max77803_platform_data exynos4_max77803_info = {
	.irq_base	= IRQ_BOARD_IFIC_START,
	.irq_gpio	= GPIO_IF_PMIC_IRQ,
	.wc_irq_gpio	= GPIO_WPC_INT,
	.wakeup		= 1,
	.muic = &max77803_muic,
	.is_default_uart_path_cp =  is_muic_default_uart_path_cp,
	.regulators = max77803_regulators,
	.num_regulators = MAX77803_REG_MAX,
#ifdef CONFIG_VIBETONZ
	.haptic_data = &max77803_haptic_pdata,
#endif

#ifdef CONFIG_LEDS_MAX77803
	.led_data = &max77803_led_pdata,
#endif
#if defined(CONFIG_CHARGER_MAX77803)
	.charger_data	= &sec_battery_pdata,
#endif
};

static struct i2c_board_info i2c_devs17_emul[] __initdata = {
	{
		I2C_BOARD_INFO("max77803", (0xCC >> 1)),
		.platform_data	= &exynos4_max77803_info,
	}
};

static struct i2c_gpio_platform_data gpio_i2c_data17 = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
};

struct platform_device s3c_device_i2c17 = {
	.name = "i2c-gpio",
	.id = 17,
	.dev.platform_data = &gpio_i2c_data17,
};

static struct platform_device *universal5410_mfd_device[] __initdata = {
	&s3c_device_i2c17,
};

#endif

#ifdef CONFIG_MFD_MAX77693

static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

static struct regulator_consumer_supply charger_supply[] = {
	REGULATOR_SUPPLY("vinchg1", "charger-manager.0"),
        REGULATOR_SUPPLY("vinchg1", NULL),
};

static struct regulator_init_data safeout1_init_data = {
	.constraints	= {
		.name		= "safeout1 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout1_supply),
	.consumer_supplies	= safeout1_supply,
};

static struct regulator_init_data safeout2_init_data = {
	.constraints	= {
		.name		= "safeout2 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 0,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout2_supply),
	.consumer_supplies	= safeout2_supply,
};

static struct regulator_init_data charger_init_data = {
	.constraints	= {
		.name		= "CHARGER",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
		REGULATOR_CHANGE_CURRENT,
		.boot_on	= 1,
		.min_uA		= 60000,
		.max_uA		= 2580000,
	},
	.num_consumer_supplies	= ARRAY_SIZE(charger_supply),
	.consumer_supplies	= charger_supply,
};

struct max77693_regulator_data max77693_regulators[] = {
        {MAX77693_ESAFEOUT1, &safeout1_init_data,},
        {MAX77693_ESAFEOUT2, &safeout2_init_data,},
        {MAX77693_CHARGER, &charger_init_data,},
};

static bool is_muic_default_uart_path_cp(void)
{
	return false;
}

#ifdef CONFIG_LEDS_MAX77693
static struct max77693_led_platform_data max77693_led_pdata = {

	.num_leds = 2,

	.leds[0].name = "leds-sec1",
	.leds[0].id = MAX77693_FLASH_LED_1,
	.leds[0].timer = MAX77693_FLASH_TIME_187P5MS,
	.leds[0].timer_mode = MAX77693_TIMER_MODE_MAX_TIMER,
	.leds[0].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[0].brightness = 0x3D,
    //.leds[0].brightness = 0x1E,

	.leds[1].name = "torch-sec1",
	.leds[1].id = MAX77693_TORCH_LED_1,
	.leds[1].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[1].brightness = 0x06,
};
#endif

#if 0
#ifdef CONFIG_VIBETONZ
static struct max77693_haptic_platform_data max77693_haptic_pdata = {
	.max_timeout = 10000,
	.duty = 35804,
	.period = 38054,
	.reg2 = MOTOR_LRA | EXT_PWM | DIVIDER_128,
	.init_hw = NULL,
	.motor_en = NULL,
	.pwm_id = 0,
	.regulator_name = "vcc_3.0v_motor",
};
#endif
#endif

struct max77693_platform_data exynos5_max77693_info = {
	.irq_base	= IRQ_BOARD_IFIC_START,
	.irq_gpio	= GPIO_IF_PMIC_IRQ,
/*	.wc_irq_gpio	= GPIO_WPC_INT,*/
	.wakeup		= 1,
	.muic = &max77693_muic,
	.is_default_uart_path_cp =  is_muic_default_uart_path_cp,
	.regulators = max77693_regulators,
	.num_regulators = MAX77693_REG_MAX,

#if 0
#ifdef CONFIG_VIBETONZ
	.haptic_data = &max77803_haptic_pdata,
#endif
#endif

#ifdef CONFIG_LEDS_MAX77693
	.led_data = &max77693_led_pdata,
#endif
#if defined(CONFIG_CHARGER_MAX77693)
	.charger_data	= &sec_battery_pdata,
#endif
};

static struct i2c_board_info i2c_devs17_emul[] __initdata = {
	{
		I2C_BOARD_INFO("max77693", (0xCC >> 1)),
		.platform_data	= &exynos5_max77693_info,
	}
};

static struct i2c_gpio_platform_data gpio_i2c_data17 = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
};

struct platform_device s3c_device_i2c17 = {
	.name = "i2c-gpio",
	.id = 17,
	.dev.platform_data = &gpio_i2c_data17,
};

static struct platform_device *vienna_mfd_device[] __initdata = {
	&s3c_device_i2c17,
};


#endif


void __init exynos5_universal5410_mfd_init(void)
{
#ifdef CONFIG_MFD_MAX77803
	i2c_register_board_info(17, i2c_devs17_emul,
				ARRAY_SIZE(i2c_devs17_emul));

	platform_add_devices(universal5410_mfd_device,
			ARRAY_SIZE(universal5410_mfd_device));
#endif
#ifdef CONFIG_MFD_MAX77693
	i2c_register_board_info(17, i2c_devs17_emul,
				ARRAY_SIZE(i2c_devs17_emul));

	platform_add_devices(vienna_mfd_device,
			ARRAY_SIZE(vienna_mfd_device));
#endif
}
