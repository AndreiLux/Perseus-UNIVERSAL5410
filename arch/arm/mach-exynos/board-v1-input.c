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
#include <plat/iic.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/regulator/machine.h>
#include "board-universal5410.h"
#include <mach/sec_debug.h>
#include <linux/wacom_i2c.h>

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#include <linux/i2c/mxts.h>

#define GPIO_TOUCH_CHG		EXYNOS5410_GPG1(2) /* interrupt pin */
#define GPIO_TOUCH_RESET	EXYNOS5410_GPJ0(0)
#define GPIO_TOUCH_EN		EXYNOS5410_GPJ1(0)
#define GPIO_TOUCH_EN_1		EXYNOS5410_GPJ0(4)
/*
  * Please locate model dependent define at below position
  * such as bootloader address, app address and firmware name
  */
#define MXT_BOOT_ADDRESS	0x26
#define MXT_APP_ADDRESS		0x4A
#define MXT_FIRMWARE_NAME_REVISION	"mXT1664S_v.fw"

/* To display configuration version on *#2663# */
#define MXT_PROJECT_NAME	"GT-P85XX"

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
	return gpio_get_value(GPIO_TOUCH_CHG);
}

static int mxt_power_on(void)
{
	struct regulator *regulator;

	/* enable I2C pullup */
	regulator = regulator_get(NULL, "tsp_vdd_1.8v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_vdd_1.8v regulator_get failed\n",
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

	/* enable VADC_3.3V pullup for XVDD */
	regulator = regulator_get(NULL, "vadc_3.3v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: vadc_3.3v regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* enable XVDD */
	gpio_set_value(GPIO_TOUCH_EN, 1);
	msleep(3);
	gpio_set_value(GPIO_TOUCH_EN_1, 1);

	msleep(50);

	usleep_range(1000, 1500);
	gpio_set_value(GPIO_TOUCH_RESET, 1);

	/* touch interrupt pin */
	s3c_gpio_cfgpin(GPIO_TOUCH_CHG, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TOUCH_CHG, S3C_GPIO_PULL_NONE);
	printk(KERN_ERR "mxt_power_on is finished\n");

	return 0;
}

static int mxt_power_off(void)
{
	struct regulator *regulator;

	gpio_set_value(GPIO_TOUCH_RESET, 0);

	/* disable XVDD */
	gpio_set_value(GPIO_TOUCH_EN, 0);
	msleep(3);
	gpio_set_value(GPIO_TOUCH_EN_1, 0);
	msleep(3);

	/* disable VADC_3.3V pullup for XVDD */
	regulator = regulator_get(NULL, "vadc_3.3v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: vadc_3.3v regulator_get failed\n",
			__func__);
		return -EIO;
	}
	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	msleep(3);

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

	/* disable I2C pullup */
	regulator = regulator_get(NULL, "tsp_vdd_1.8v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_vdd_1.8v regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* touch interrupt pin */
	s3c_gpio_cfgpin(GPIO_TOUCH_CHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TOUCH_CHG, S3C_GPIO_PULL_NONE);
	printk(KERN_ERR "mxt_power_off is finished!\n");

	return 0;
}

#if ENABLE_TOUCH_KEY
struct mxt_touchkey mxt_touchkeys[] = {
	{
		.value = 0x20,
		.keycode = KEY_DUMMY_MENU,
		.name = "d_menu",
		.xnode = 31, /* first x num conut is 0 */
		.ynode = 51, /* first y num conut is 0 */
		.deltaobj = 5, /*base on datasheet*/
	},
	{
		.value = 0x10,
		.keycode = KEY_MENU,
		.name = "menu",
		.xnode = 30,
		.ynode = 51,
		.deltaobj = 4,
	},
	{
		.value = 0x08,
		.keycode = KEY_DUMMY_HOME1,
		.name = "d_home1",
		.xnode = 29,
		.ynode = 51,
		.deltaobj = 3,
	},
	{
		.value = 0x04,
		.keycode = KEY_DUMMY_HOME2,
		.name = "d_home2",
		.xnode = 28,
		.ynode = 51,
		.deltaobj = 2,
	},
	{
		.value = 0x02,
		.keycode = KEY_BACK,
		.name = "back",
		.xnode = 27,
		.ynode = 51,
		.deltaobj = 1,
	},
	{
		.value = 0x01,
		.keycode = KEY_DUMMY_BACK,
		.name = "d_back",
		.xnode = 26,
		.ynode = 51,
		.deltaobj = 0,
	},
};

static int mxt_led_power_on(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, "key_led_3.3v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: key_led_3.3v regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	printk(KERN_ERR "mxt_led_power_on is finished\n");
	return 0;
}

static int mxt_led_power_off(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, "key_led_3.3v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: key_led_3.3v regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	printk(KERN_ERR "mxt_led_power_off is finished\n");
	return 0;
}
#endif

static void mxt_gpio_init(void)
{
	/* touch interrupt */
	gpio_request(GPIO_TOUCH_CHG, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TOUCH_CHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TOUCH_CHG, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_TOUCH_CHG);

	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);

	gpio_request_one(GPIO_TOUCH_RESET, GPIOF_OUT_INIT_LOW, "atmel_mxt_ts nRESET");

	gpio_request(GPIO_TOUCH_EN, "GPIO_TOUCH_EN");
	s3c_gpio_cfgpin(GPIO_TOUCH_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TOUCH_EN, S3C_GPIO_PULL_NONE);

	gpio_request(GPIO_TOUCH_EN_1, "GPIO_TOUCH_EN_1");
	s3c_gpio_cfgpin(GPIO_TOUCH_EN_1, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TOUCH_EN_1, S3C_GPIO_PULL_NONE);

	s5p_gpio_set_pd_cfg(GPIO_TOUCH_CHG, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_pull(GPIO_TOUCH_CHG, S5P_GPIO_PD_UPDOWN_DISABLE);
	s5p_gpio_set_pd_cfg(GPIO_TOUCH_RESET, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(GPIO_TOUCH_EN, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(GPIO_TOUCH_EN_1, S5P_GPIO_PD_PREV_STATE);
}

static struct mxt_platform_data mxt_data = {
	.num_xnode = 32,
	.num_ynode = 52,
	.max_x = 4095,
	.max_y = 4095,
	.irqflags = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.boot_address = MXT_BOOT_ADDRESS,
	.project_name = MXT_PROJECT_NAME,
	.revision = MXT_REVISION_G,
	.read_chg = mxt_read_chg,
	.power_on = mxt_power_on,
	.power_off = mxt_power_off,
	.register_cb = mxt_register_callback,
	.firmware_name = MXT_FIRMWARE_NAME_REVISION,
#if ENABLE_TOUCH_KEY
	.num_touchkey = ARRAY_SIZE(mxt_touchkeys),
	.touchkey = mxt_touchkeys,
	.led_power_on = mxt_led_power_on,
	.led_power_off = mxt_led_power_off,
#endif
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

	mxt_i2c_devs0[0].irq = gpio_to_irq(GPIO_TOUCH_CHG);

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, mxt_i2c_devs0, ARRAY_SIZE(mxt_i2c_devs0));

	printk(KERN_ERR "%s touch : %d [%d]\n",
		 		__func__, mxt_i2c_devs0[0].irq, system_rev);
}

#endif

static struct wacom_g5_callbacks *wacom_callbacks;

static int wacom_early_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N
	gpio_set_value(GPIO_PEN_RESET_N, 0);
#endif
	gpio_direction_output(GPIO_PEN_LDO_EN, 0);
	/* Set GPIO_PEN_IRQ to pull-up to reduce leakage */
	s3c_gpio_setpull(GPIO_PEN_IRQ, S3C_GPIO_PULL_UP);

	return 0;
}

static int wacom_late_resume_hw(void)
{
	gpio_direction_output(GPIO_PEN_PDCT, 1);
	s3c_gpio_setpull(GPIO_PEN_IRQ, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_PEN_LDO_EN, 1);
	msleep(100);
	gpio_direction_input(GPIO_PEN_PDCT);
#ifdef GPIO_PEN_RESET_N
	gpio_set_value(GPIO_PEN_RESET_N, 1);
#endif
	return 0;
}

static int wacom_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N
	gpio_set_value(GPIO_PEN_RESET_N, 0);
#endif
	gpio_direction_output(GPIO_PEN_LDO_EN, 0);
	/* Set GPIO_PEN_IRQ to pull-up to reduce leakage */
	s3c_gpio_setpull(GPIO_PEN_IRQ, S3C_GPIO_PULL_UP);
	return 0;
}

static int wacom_resume_hw(void)
{
	gpio_direction_output(GPIO_PEN_PDCT, 1);
	s3c_gpio_setpull(GPIO_PEN_IRQ, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_PEN_LDO_EN, 1);
	/*msleep(100);*/
	gpio_direction_input(GPIO_PEN_PDCT);
#ifdef GPIO_PEN_RESET_N
	gpio_set_value(GPIO_PEN_RESET_N, 1);
#endif
	return 0;
}

static int wacom_reset_hw(void)
{
	wacom_suspend_hw();
	msleep(100);
	wacom_resume_hw();

	return 0;
}

static void wacom_register_callbacks(struct wacom_g5_callbacks *cb)
{
	wacom_callbacks = cb;
};

#ifdef WACOM_HAVE_FWE_PIN
static void wacom_compulsory_flash_mode(bool en)
{
	gpio_set_value(GPIO_PEN_FWE1, en);
}
#endif


static struct wacom_g5_platform_data wacom_platform_data = {
	.x_invert = WACOM_X_INVERT,
	.y_invert = WACOM_Y_INVERT,
	.xy_switch = WACOM_XY_SWITCH,
	.min_x = 0,
	.max_x = WACOM_POSX_MAX,
	.min_y = 0,
	.max_y = WACOM_POSY_MAX,
	.min_pressure = 0,
	.max_pressure = WACOM_PRESSURE_MAX,
	.gpio_pendct = GPIO_PEN_PDCT,
	/*.init_platform_hw = wacom_init,*/
	/*      .exit_platform_hw =,    */
	.suspend_platform_hw = wacom_suspend_hw,
	.resume_platform_hw = wacom_resume_hw,
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend_platform_hw = wacom_early_suspend_hw,
	.late_resume_platform_hw = wacom_late_resume_hw,
#endif
	.reset_platform_hw = wacom_reset_hw,
	.register_cb = wacom_register_callbacks,
	.compulsory_flash_mode = wacom_compulsory_flash_mode,
#ifdef WACOM_PEN_DETECT
	.gpio_pen_insert = GPIO_PEN_DETECT,
#endif
};

/* I2C */
static struct i2c_board_info wacom_i2c_devs[] __initdata = {
	{
		I2C_BOARD_INFO("wacom_g5sp_i2c", 0x56),
		.platform_data = &wacom_platform_data,
	},
};

#define WACOM_SET_I2C(ch, pdata, i2c_info)	\
do {		\
	s3c_i2c##ch##_set_platdata(pdata);	\
	i2c_register_board_info(ch, i2c_info,	\
		ARRAY_SIZE(i2c_info));	\
	platform_device_register(&s3c_device_i2c##ch);	\
} while (0);

void __init wacom_init(void)
{
	int gpio;
	int ret;

	/*SLP & FWE1*/
	printk(KERN_INFO "epen:Use FWE\n");
	gpio = GPIO_PEN_FWE1;
	ret = gpio_request(gpio, "PEN_FWE1");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_FWE1.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_direction_output(gpio, 0);

	/*PDCT*/
	gpio = GPIO_PEN_PDCT;
	ret = gpio_request(gpio, "PEN_PDCT");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_PDCT.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);
	gpio_direction_input(gpio);

	irq_set_irq_type(gpio_to_irq(gpio), IRQ_TYPE_EDGE_BOTH);

	/*IRQ*/
	gpio = GPIO_PEN_IRQ;
	ret = gpio_request(gpio, "PEN_IRQ");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_IRQ.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);
	gpio_direction_input(gpio);

	wacom_i2c_devs[0].irq = gpio_to_irq(gpio);
	irq_set_irq_type(wacom_i2c_devs[0].irq, IRQ_TYPE_EDGE_RISING);

	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));

	/*LDO_EN*/
	gpio = GPIO_PEN_LDO_EN;
	ret = gpio_request(gpio, "PEN_LDO_EN");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_LDO_EN.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	gpio_direction_output(gpio, 0);

#if defined (CONFIG_V1_3G_REV00) || defined(CONFIG_V1_WIFI_REV00)  || defined(CONFIG_V1_3G_REV03)
	WACOM_SET_I2C(3, NULL, wacom_i2c_devs);
#else
	WACOM_SET_I2C(2, NULL, wacom_i2c_devs);
#endif

	printk(KERN_INFO "epen:: wacom IC initialized.\n");
}

static void universal5410_gpio_keys_config_setup(void)
{
	s3c_gpio_setpull(EXYNOS5410_GPX0(2), S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(EXYNOS5410_GPX0(3), S3C_GPIO_PULL_UP);
}

static struct gpio_keys_button universal5410_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_nPOWER,
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOL_UP,
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOL_DOWN,
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_KEY_HOME,
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
	},
};

static struct gpio_keys_platform_data universal5410_gpiokeys_platform_data = {
	universal5410_button,
	ARRAY_SIZE(universal5410_button),
#ifdef CONFIG_SENSORS_HALL
	.gpio_flip_cover = GPIO_HALL_SENSOR_INT,
#endif
};

static struct platform_device universal5410_gpio_keys = {
	.name   = "gpio-keys",
	.dev    = {
		.platform_data = &universal5410_gpiokeys_platform_data,
	},
};
void __init exynos5_universal5410_input_init(void)
{

	universal5410_gpio_keys_config_setup();
	platform_device_register(&universal5410_gpio_keys);

	atmel_tsp_init();
	if (lpcharge == 0) {
		platform_device_register(&s3c_device_i2c0);
	} else {
		printk(KERN_INFO "%s lpm mode : Do not register tsp driver.", __func__);
	}

	wacom_init();

#ifdef CONFIG_SENSORS_HALL
	gpio_request(GPIO_HALL_SENSOR_INT, "GPIO_HALL_SENSOR_INT");
	s3c_gpio_cfgpin(GPIO_HALL_SENSOR_INT, S3C_GPIO_SFN(0xf));
	s5p_register_gpio_interrupt(GPIO_HALL_SENSOR_INT);
	gpio_direction_input(GPIO_HALL_SENSOR_INT);
#endif
}
