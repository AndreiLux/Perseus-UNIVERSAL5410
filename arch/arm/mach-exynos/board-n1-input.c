/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * ---------------------------------------------------
 * N1 input device lists
 * ---------------------------------------------------
 * tsp : atmel mxt1664s
 * hall sensor / keys
 * wacom : w9007
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
#include <mach/hs-iic.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>
#include <linux/i2c-gpio.h>
#include "board-universal5420.h"
#include <mach/sec_debug.h>

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#include <linux/i2c/mxts.h>
#endif

#ifdef CONFIG_INPUT_WACOM
#include <linux/wacom_i2c.h>
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);
extern unsigned int system_rev;

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#define MXT_BOOT_ADDRESS	0x26
#define MXT_APP_ADDRESS		0x4A
#define MXT_FIRMWARE_NAME_REVISION	"mXT1664S_n.fw"
#if defined(CONFIG_N1A_WIFI)
#define MXT_PROJECT_NAME	"SM-P600"
#else
#define MXT_PROJECT_NAME	"SM-P601"
#endif


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

    /* temporary reconfig gpios */
	s3c_gpio_cfgpin(GPIO_TOUCH_CHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TOUCH_CHG, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_TOUCH_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TOUCH_EN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_TOUCH_EN_1, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TOUCH_EN_1, S3C_GPIO_PULL_NONE);

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
	printk(KERN_ERR "[TSP] power on\n");

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
	printk(KERN_ERR "[TSP] power off\n");

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

	printk(KERN_ERR "[KEYLED] on\n");
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

	printk(KERN_ERR "[KEYLED] off\n");
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
 
#ifdef CONFIG_INPUT_WACOM
static struct wacom_g5_callbacks *wacom_callbacks;
static bool wacom_power_enabled;

int wacom_power(bool on)
{
#ifdef GPIO_PEN_LDO_EN
	gpio_direction_output(GPIO_PEN_LDO_EN, on);
#else
	struct regulator *regulator_vdd;

	if (wacom_power_enabled == on) {
		printk(KERN_DEBUG "epen: %s %s\n",
			__func__, on ? "on" : "off");
		return 0;
	}

	regulator_vdd = regulator_get(NULL, "wacom_3.0v");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR"epen: %s reg get err\n", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);
	printk(KERN_DEBUG "epen: %s %s\n",
		__func__, on ? "on" : "off");

	wacom_power_enabled = on;
#endif
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static int wacom_early_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 0);
#endif
	wacom_power(0);

	return 0;
}

static int wacom_late_resume_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 1);
#endif
	gpio_direction_output(GPIO_PEN_PDCT_18V, 1);
	wacom_power(1);
	msleep(100);
	gpio_direction_input(GPIO_PEN_PDCT_18V);
	return 0;
}
#endif

static int wacom_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 0);
#endif
	wacom_power(0);
	return 0;
}

static int wacom_resume_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 1);
#endif
	gpio_direction_output(GPIO_PEN_PDCT_18V, 1);
	wacom_power(1);
	/*msleep(100);*/
	gpio_direction_input(GPIO_PEN_PDCT_18V);
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

static void wacom_compulsory_flash_mode(bool en)
{
	gpio_direction_output(GPIO_PEN_FWE1_18V, en);
}

static struct wacom_g5_platform_data wacom_platform_data = {
	.x_invert = WACOM_X_INVERT,
	.y_invert = WACOM_Y_INVERT,
	.xy_switch = WACOM_XY_SWITCH,
	.min_x = 0,
	.max_x = WACOM_MAX_COORD_X,
	.min_y = 0,
	.max_y = WACOM_MAX_COORD_Y,
	.min_pressure = 0,
	.max_pressure = WACOM_MAX_PRESSURE,
	.gpio_pendct = GPIO_PEN_PDCT_18V,
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
	.gpio_pen_insert = GPIO_WACOM_SENSE,
};

/* I2C */
static struct i2c_board_info wacom_i2c_devs[] __initdata = {
	{
		I2C_BOARD_INFO("wacom_g5sp_i2c", WACOM_I2C_ADDR),
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

#ifdef GPIO_PEN_RESET_N_18V
	/*Reset*/
	gpio = GPIO_PEN_RESET_N_18V;
	ret = gpio_request(gpio, "PEN_RESET_N");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_RESET_N.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_direction_output(gpio, 0);
#endif

	/*SLP & FWE1*/
	gpio = GPIO_PEN_FWE1_18V;
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
	gpio = GPIO_PEN_PDCT_18V;
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
	gpio = GPIO_PEN_IRQ_18V;
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
#ifdef GPIO_PEN_LDO_EN
	gpio = GPIO_PEN_LDO_EN;
	ret = gpio_request(gpio, "PEN_LDO_EN");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_LDO_EN.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	gpio_direction_output(gpio, 0);
#else
	wacom_power(0);
#endif

	/*WACOM_SET_I2C(3, NULL, wacom_i2c_devs);*/
	exynos5_hs_i2c4_set_platdata(NULL);
	i2c_register_board_info(8, wacom_i2c_devs, ARRAY_SIZE(wacom_i2c_devs));

	printk(KERN_INFO "epen:: wacom IC initialized.\n");
}
#endif

static void universal5410_gpio_keys_config_setup(u32 system_rev)
{
	pr_info("gpio_keys : system_rev %d\n", system_rev);

	if (0x0 == system_rev)
		s3c_gpio_setpull(GPIO_HOME_KEY, S3C_GPIO_PULL_UP);
};

#define GPIO_KEYS(_name, _code, _gpio, _active_low, _iswake)	\
{									\
	.desc = _name,					\
	.code = _code,					\
	.gpio = _gpio,					\
	.active_low = _active_low,			\
	.type = EV_KEY,					\
	.wakeup = _iswake,				\
	.debounce_interval = 10,			\
	.value = 1						\
}

static struct gpio_keys_button universal5410_button[] = {
	GPIO_KEYS("KEY_POWER", KEY_POWER,
		GPIO_nPOWER, true, true),
	GPIO_KEYS("KEY_VOLUMEUP", KEY_VOLUMEUP,
		GPIO_VOL_UP, true, false),
	GPIO_KEYS("KEY_VOLUMEDOWN", KEY_VOLUMEDOWN,
		GPIO_VOL_DOWN, true, false),
	GPIO_KEYS("KEY_HOME", KEY_HOMEPAGE,
		GPIO_HOME_KEY, true, true),
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

static struct input_debug_key_state kstate[] = {
	SET_DEBUG_KEY(KEY_POWER, false),
	SET_DEBUG_KEY(KEY_VOLUMEUP, false),
	SET_DEBUG_KEY(KEY_VOLUMEDOWN, true),
	SET_DEBUG_KEY(KEY_HOMEPAGE, false),
};

static struct input_debug_pdata input_debug_platform_data = {
	.nkeys = ARRAY_SIZE(kstate),
	.key_state = kstate,
};

static struct platform_device input_debug = {
	.name	= SEC_DEBUG_NAME,
	.dev	= {
		.platform_data = &input_debug_platform_data,
	},
};

static struct platform_device *universal5410_input_devices[] __initdata = {
	&universal5410_gpio_keys,
	&input_debug,
#ifdef CONFIG_INPUT_WACOM
	&exynos5_device_hs_i2c4,
#endif
};

void __init exynos5_universal5420_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXTS)
	atmel_tsp_init();
    //tsp_check_and_init();
#endif
	universal5410_gpio_keys_config_setup(system_rev);

#ifdef CONFIG_INPUT_WACOM
	wacom_init();
#endif

#ifdef CONFIG_SENSORS_HALL
	s3c_gpio_setpull(GPIO_HALL_SENSOR_INT, S3C_GPIO_PULL_UP);
	gpio_request(GPIO_HALL_SENSOR_INT, "GPIO_HALL_SENSOR_INT");
	s3c_gpio_cfgpin(GPIO_HALL_SENSOR_INT, S3C_GPIO_SFN(0xf));
	s5p_register_gpio_interrupt(GPIO_HALL_SENSOR_INT);
	gpio_direction_input(GPIO_HALL_SENSOR_INT);
#endif

	if (lpcharge == 0) {
		platform_device_register(&s3c_device_i2c0);
	} else {
		printk(KERN_INFO "%s lpm mode : Do not register tsp driver.", __func__);
	}

	platform_add_devices(universal5410_input_devices,
			ARRAY_SIZE(universal5410_input_devices));
}
