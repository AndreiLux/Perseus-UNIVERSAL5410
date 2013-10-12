/*
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
#include <linux/gpio_keys.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#include <linux/i2c/mxts.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
#include <linux/i2c/synaptics_rmi.h>
#include <linux/interrupt.h>
#endif

#include <plat/iic.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>

#ifdef CONFIG_TOUCHSCREEN_MELFAS
#include <mach/midas-tsp.h>
#endif
#include <mach/gpio.h>

#include <linux/i2c-gpio.h>

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
#include <linux/i2c/touchkey_i2c.h>
#endif

#include "board-universal5410.h"

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
static struct i2c_board_info i2c_devs8_emul[];

static void touchkey_init_hw(void)
{
#ifndef LED_LDO_WITH_REGULATOR
	gpio_request(GPIO_3_TOUCH_EN, "gpio_3_touch_en");
#endif
	gpio_request(EXYNOS5410_GPH1(3), "2_TOUCH_INT");
	s3c_gpio_setpull(EXYNOS5410_GPH1(3), S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(EXYNOS5410_GPH1(3));
	gpio_direction_input(EXYNOS5410_GPH1(3));

	i2c_devs8_emul[0].irq = gpio_to_irq(EXYNOS5410_GPH1(3));
	irq_set_irq_type(gpio_to_irq(EXYNOS5410_GPH1(3)), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(EXYNOS5410_GPH1(3), S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(EXYNOS5410_GPK0(6), S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(EXYNOS5410_GPK0(7), S3C_GPIO_PULL_DOWN);
}

static int touchkey_suspend(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_suspend : TK regulator_get failed\n");
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_force_disable(regulator);

	s3c_gpio_setpull(EXYNOS5410_GPK0(6), S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(EXYNOS5410_GPK0(7), S3C_GPIO_PULL_DOWN);

	regulator_put(regulator);

	return 1;
}

static int touchkey_resume(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_resume : TK regulator_get failed\n");
		return -EIO;
	}

	regulator_enable(regulator);
	regulator_put(regulator);

	s3c_gpio_setpull(EXYNOS5410_GPK0(6), S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(EXYNOS5410_GPK0(7), S3C_GPIO_PULL_NONE);

	return 1;
}

static int touchkey_power_on(bool on)
{
	int ret;

	if (on) {
		gpio_direction_output(EXYNOS5410_GPH1(3), 1);
		irq_set_irq_type(gpio_to_irq(EXYNOS5410_GPH1(3)),
			IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(EXYNOS5410_GPH1(3), S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(EXYNOS5410_GPH1(3), S3C_GPIO_PULL_NONE);
	} else
		gpio_direction_input(EXYNOS5410_GPH1(3));

	if (on)
		ret = touchkey_resume();
	else
		ret = touchkey_suspend();

	return ret;
}

static int touchkey_led_power_on(bool on)
{
#ifdef LED_LDO_WITH_REGULATOR
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR
			"[Touchkey] touchkey_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR
			"[Touchkey] touchkey_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}
#else
	if (on)
		gpio_direction_output(GPIO_3_TOUCH_EN, 1);
	else
		gpio_direction_output(GPIO_3_TOUCH_EN, 0);
#endif
	return 1;
}

static struct touchkey_platform_data touchkey_pdata = {
	.gpio_sda = EXYNOS5410_GPK0(7),
	.gpio_scl = EXYNOS5410_GPK0(6),
	.gpio_int = EXYNOS5410_GPH1(3),
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
};

static struct i2c_gpio_platform_data gpio_i2c_data8 = {
	.sda_pin = EXYNOS5410_GPK0(7),
	.scl_pin = EXYNOS5410_GPK0(6),
};

struct platform_device s3c_device_i2c8 = {
	.name = "i2c-gpio",
	.id = 8,
	.dev.platform_data = &gpio_i2c_data8,
};

/* I2C8 */
static struct i2c_board_info i2c_devs8_emul[] = {
	{
		I2C_BOARD_INFO("sec_touchkey", 0x20),
		.platform_data = &touchkey_pdata,
	},
};
#endif /*CONFIG_KEYBOARD_CYPRESS_TOUCH*/

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
/*
 * Please locate model dependent define at below position
 * such as bootloader address, app address and firmware name
 */
#if defined(CONFIG_UNIVERSAL5410_REV04)
#define MXT_BOOT_ADDRESS	0x25
#define MXT_APP_ADDRESS		0x4B
#else
#define MXT_BOOT_ADDRESS	0x24
#define MXT_APP_ADDRESS		0x4A
#endif

/* H/W revision to support Revision S */
#if defined(CONFIG_TARGET_LOCALE_KOR)
#define MXT_SUPPORT_REV_S      11
#else
#define MXT_SUPPORT_REV_S      12
#endif


/* We need to support two types of IC revision at once,
 * So two firmwwares are loaded, and we need to add proper firmware name
 * to platform data according to revision of IC.
 *
 * REV_G : Firmware version is like 1.x.
 * REV_I : Firmware version is like 2.x and it added Hovering functionality
 * compared with REV_G.
 */
#define MXT_FIRMWARE_NAME_REVISION_G	"mXT540Sg.fw"
#define MXT_FIRMWARE_NAME_REVISION_I	"mXT540Si.fw"

/* To display configuration version on *#2663# */
#define MXT_PROJECT_NAME	"GT-I95XX"

static struct mxt_callbacks *charger_callbacks;

void tsp_charger_infom(bool en)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, en);
}

static void mxt_register_callback(void *cb)
{
	charger_callbacks = cb;
}

static bool mxt_read_chg(void)
{
	return gpio_get_value(EXYNOS5410_GPJ0(0));
}

static int mxt_power_on(void)
{
	struct regulator *regulator;

	/* enable I2C pullup */
	regulator = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_vdd regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* enable DVDD */
	regulator = regulator_get(NULL, "tsp_vdd_3.0v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_dvdd regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* enable AVDD */
	regulator = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_avdd regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* touch interrupt pin */
	s3c_gpio_cfgpin(EXYNOS5410_GPJ0(0), S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(EXYNOS5410_GPJ0(0), S3C_GPIO_PULL_NONE);

	printk(KERN_ERR "mxt_power_on is finished\n");

	return 0;
}

static int mxt_power_off(void)
{
	struct regulator *regulator;

	/* disable AVDD */
	regulator = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_avdd regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* disable DVDD */
	regulator = regulator_get(NULL, "tsp_vdd_3.0v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_dvdd regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* disable I2C pullup */
	regulator = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_vdd regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* touch interrupt pin */
	s3c_gpio_cfgpin(EXYNOS5410_GPJ0(0), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS5410_GPJ0(0), S3C_GPIO_PULL_NONE);

	printk(KERN_ERR "mxt_power_off is finished\n");

	return 0;
}

static void mxt_gpio_init(void)
{
	/* touch interrupt */
	gpio_request(EXYNOS5410_GPJ0(0), "EXYNOS5410_GPJ0(0)");
	s3c_gpio_cfgpin(EXYNOS5410_GPJ0(0), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS5410_GPJ0(0), S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(EXYNOS5410_GPJ0(0));

	s3c_gpio_setpull(EXYNOS5410_GPB3(1), S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(EXYNOS5410_GPB3(0), S3C_GPIO_PULL_NONE);
}

static struct mxt_platform_data mxt_data = {
	.num_xnode = 27,
	.num_ynode = 15,
	.max_x = 4095,
	.max_y = 4095,
	.irqflags = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.boot_address = MXT_BOOT_ADDRESS,
	.firmware_name = MXT_FIRMWARE_NAME_REVISION_G,
	.project_name = MXT_PROJECT_NAME,
	.revision = MXT_REVISION_G,
	.read_chg = mxt_read_chg,
	.power_on = mxt_power_on,
	.power_off = mxt_power_off,
	.register_cb = mxt_register_callback,
};

static struct i2c_board_info mxt_i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO(MXT_DEV_NAME, MXT_APP_ADDRESS),
		.platform_data = &mxt_data,
	}
};

void __init atmel_tsp_init(void)
{
	mxt_gpio_init();

	mxt_i2c_devs0[0].irq = gpio_to_irq(EXYNOS5410_GPJ0(0));

	/* Revision_I is applied from H/W revision(1.1).*/
	if (system_rev >= MXT_SUPPORT_REV_S) {
		mxt_data.num_xnode = 28;
		mxt_data.num_ynode = 16;
		mxt_data.firmware_name = MXT_FIRMWARE_NAME_REVISION_I;
		mxt_data.revision = MXT_REVISION_I;
	}
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, mxt_i2c_devs0,
			ARRAY_SIZE(mxt_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		 __func__, mxt_i2c_devs0[0].irq);
}
#endif

/*	Synaptics Thin Driver	*/
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
static int synaptics_power(bool on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;
	static bool enabled;

	if (enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_vdd regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	regulator_avdd = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd regulator_get failed\n");
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_vdd);
		regulator_enable(regulator_avdd);
	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);

	return 0;
}

static int synaptics_gpio_setup(unsigned gpio, bool configure)
{
	int retval = 0;

	if (configure) {
		gpio_request(gpio, "TSP_INT");
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(gpio);
	} else {
		pr_warn("%s: No way to deconfigure gpio %d.",
		       __func__, gpio);
	}

	return retval;
}
#ifdef NO_0D_WHILE_2D

static unsigned char tm1940_f1a_button_codes[] = {KEY_MENU, KEY_BACK};

static struct synaptics_rmi_f1a_button_map tm1940_f1a_button_map = {
	.nbuttons = ARRAY_SIZE(tm1940_f1a_button_codes),
	.map = tm1940_f1a_button_codes,
};

static int ts_led_power_on(bool on)
{
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, "touchkey_led");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "touchkey_led");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}
#endif

#define TM1940_ADDR 0x20
#define TM1940_ATTN 130

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.irq_type = IRQF_TRIGGER_FALLING,
	.gpio = EXYNOS5410_GPJ0(0),
	.power = synaptics_power,
	.gpio_config = synaptics_gpio_setup,
#ifdef NO_0D_WHILE_2D
	.led_power_on = ts_led_power_on,
	.f1a_button_map = &tm1940_f1a_button_map,
#endif

};

static struct i2c_board_info synaptics_i2c_devs0[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
	}
};

void __init synaptics_tsp_init(void)
{
	gpio_request(EXYNOS5410_GPJ0(0), "TSP_INT");
	s3c_gpio_cfgpin(EXYNOS5410_GPJ0(0), S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(EXYNOS5410_GPJ0(0), S3C_GPIO_PULL_UP);
	s5p_register_gpio_interrupt(EXYNOS5410_GPJ0(0));

	synaptics_i2c_devs0[0].irq = gpio_to_irq(EXYNOS5410_GPJ0(0));

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, synaptics_i2c_devs0,
		 ARRAY_SIZE(synaptics_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		 __func__, synaptics_i2c_devs0[0].irq);
}
#endif

static void universal5410_gpio_keys_config_setup(void)
{
	s3c_gpio_setpull(EXYNOS5410_GPX0(2), S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(EXYNOS5410_GPX0(3), S3C_GPIO_PULL_UP);
}

static struct gpio_keys_button universal5410_button[] = {
	{
		.code = KEY_POWER,
		.gpio = EXYNOS5410_GPX2(2),
		.active_low = 1,
		.wakeup = 1,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = EXYNOS5410_GPX0(2),
		.active_low = 1,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = EXYNOS5410_GPX0(3),
		.active_low = 1,
	},
};

static struct gpio_keys_platform_data universal5410_gpiokeys_platform_data = {
	universal5410_button,
	ARRAY_SIZE(universal5410_button),
};

static struct platform_device universal5410_gpio_keys = {
	.name   = "gpio-keys",
	.dev    = {
		.platform_data = &universal5410_gpiokeys_platform_data,
	},
};

static struct platform_device *universal5410_input_devices[] __initdata = {
	&s3c_device_i2c0,
	&universal5410_gpio_keys,
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	&s3c_device_i2c8,
#endif
};

void __init tsp_check_and_init(void)
{
	u8 touch_type = (lcdtype & 0x7000) >> 12;
	/* 6~4 bit of LCD ID2 represent type of touch IC */

	printk(KERN_ERR "%s system_rev:%d, lcdtype: %06X, touch_type : %d\n",
		 __func__, system_rev, lcdtype, touch_type);

	/* Touch ID defines
	 * 0 = Atmel mxt540s_revG
	 * 1 = Synaptic IC
	 * 2 = Atmel mxt540s_revS Boot only
	 * 3 = Atmel mxt540s_revS Pre alpha
	 * 4 = Synaptic IC */

	if (system_rev >= 12) {
		if (touch_type == 1 || touch_type == 4)
			synaptics_tsp_init();
		else
			atmel_tsp_init();
	} else {
		atmel_tsp_init();
	}
}

void __init exynos5_universal5410_input_init(void)
{
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXTS) || \
	defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
	tsp_check_and_init();
#elif defined(CONFIG_TOUCHSCREEN_MELFAS)
	s3c_i2c0_set_platdata(NULL);
	midas_tsp_init();
#endif
	universal5410_gpio_keys_config_setup();
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	touchkey_init_hw();
	i2c_register_board_info(8, i2c_devs8_emul, ARRAY_SIZE(i2c_devs8_emul));
#endif

	platform_add_devices(universal5410_input_devices,
			ARRAY_SIZE(universal5410_input_devices));
}
