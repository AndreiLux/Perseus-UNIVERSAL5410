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

#ifdef CONFIG_TOUCHSCREEN_MELFAS
#include <mach/midas-tsp.h>
#endif
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
#include <linux/i2c/touchkey_i2c.h>
#endif

#include <linux/regulator/machine.h>
#include "board-universal5410.h"
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#include <linux/i2c/mxts.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
#include <linux/i2c/synaptics_rmi.h>
#include <linux/interrupt.h>
#endif

#include <mach/sec_debug.h>

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
static struct i2c_board_info i2c_devs8_emul[];

static struct touchkey_callbacks *tk_charger_callbacks;

void touchkey_charger_infom(bool en)
{
	if (tk_charger_callbacks && tk_charger_callbacks->inform_charger)
		tk_charger_callbacks->inform_charger(tk_charger_callbacks, en);
}

static void touchkey_register_callback(void *cb)
{
	tk_charger_callbacks = cb;
}

static void touchkey_init_hw(void)
{
#ifndef LED_LDO_WITH_REGULATOR
	gpio_request(GPIO_3_TOUCH_EN, "gpio_3_touch_en");
#endif
	gpio_request(GPIO_2_TOUCH_INT, "2_TOUCH_INT");
	s3c_gpio_setpull(GPIO_2_TOUCH_INT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_2_TOUCH_INT);
	gpio_direction_input(GPIO_2_TOUCH_INT);

	i2c_devs8_emul[0].irq = gpio_to_irq(GPIO_2_TOUCH_INT);
	irq_set_irq_type(gpio_to_irq(GPIO_2_TOUCH_INT), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(GPIO_2_TOUCH_INT, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(GPIO_2_TOUCH_SCL_18V, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_2_TOUCH_SDA_18V, S3C_GPIO_PULL_DOWN);
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

	s3c_gpio_setpull(GPIO_2_TOUCH_SCL_18V, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_2_TOUCH_SDA_18V, S3C_GPIO_PULL_DOWN);

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

	s3c_gpio_setpull(GPIO_2_TOUCH_SCL_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_2_TOUCH_SDA_18V, S3C_GPIO_PULL_NONE);

	return 1;
}

static int touchkey_power_on(bool on)
{
	int ret;

	if (on) {
		gpio_direction_output(GPIO_2_TOUCH_INT, 1);
		irq_set_irq_type(gpio_to_irq(GPIO_2_TOUCH_INT),
			IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(GPIO_2_TOUCH_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_2_TOUCH_INT, S3C_GPIO_PULL_NONE);
	} else
		gpio_direction_input(GPIO_2_TOUCH_INT);

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
	.gpio_sda = GPIO_2_TOUCH_SDA_18V,
	.gpio_scl = GPIO_2_TOUCH_SCL_18V,
	.gpio_int = GPIO_2_TOUCH_INT,
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
	.register_cb = touchkey_register_callback,
};

static struct i2c_gpio_platform_data gpio_i2c_data8 = {
	.sda_pin = GPIO_2_TOUCH_SDA_18V,
	.scl_pin = GPIO_2_TOUCH_SCL_18V,
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

#define GPIO_TSP_nINT_SECURE	EXYNOS5410_GPX1(6)

static void touch_i2c0_cfg_gpio(struct platform_device *dev)
{
	s3c_gpio_cfgall_range(GPIO_TSP_SDA_18V, 2,
				  S3C_GPIO_SFN(2), S3C_GPIO_PULL_NONE);
};

static struct s3c2410_platform_i2c touch_i2c0_platdata __initdata = {
	.bus_num	= 0,
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 400*1000,
	.sda_delay	= 100,
	.cfg_gpio	= touch_i2c0_cfg_gpio,
};

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
/*
  * Please locate model dependent define at below position
  * such as bootloader address, app address and firmware name
  */
#if defined(CONFIG_UNIVERSAL5410_REV04) \
	   || defined(CONFIG_UNIVERSAL5410_3G_REV04)
#define MXT_BOOT_ADDRESS	0x25
#define MXT_APP_ADDRESS		0x4B
#else
#define MXT_BOOT_ADDRESS	0x24
#define MXT_APP_ADDRESS		0x4A
#endif

/* H/W revision to support Revision S */
#if defined(CONFIG_TARGET_LOCALE_KOR)
#define MXT_SUPPORT_REV_S	11
#else
#define MXT_SUPPORT_REV_S	12
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
	return gpio_get_value(GPIO_TSP_nINT);
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
	s3c_gpio_cfgpin(GPIO_TSP_nINT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_nINT, S3C_GPIO_PULL_NONE);

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
	s3c_gpio_cfgpin(GPIO_TSP_nINT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_nINT, S3C_GPIO_PULL_NONE);

	printk(KERN_ERR "mxt_power_off is finished\n");

	return 0;
}

static void mxt_gpio_init(void)
{
	/* touch interrupt */
	gpio_request(GPIO_TSP_nINT, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TSP_nINT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_nINT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_TSP_nINT);

	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);
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

	mxt_i2c_devs0[0].irq = gpio_to_irq(GPIO_TSP_nINT);

	/* Revision_I is applied from H/W revision(1.1).*/
	if (system_rev >= MXT_SUPPORT_REV_S) {
		mxt_data.num_xnode = 28;
		mxt_data.num_ynode = 16;
		mxt_data.firmware_name = MXT_FIRMWARE_NAME_REVISION_I;
		mxt_data.revision = MXT_REVISION_I;
	}
	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
	i2c_register_board_info(0, mxt_i2c_devs0,
			ARRAY_SIZE(mxt_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		 __func__, mxt_i2c_devs0[0].irq);
}
#endif

/*	Synaptics Thin Driver	*/
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
#ifdef CONFIG_SEC_TSP_FACTORY
unsigned int bootmode;
EXPORT_SYMBOL(bootmode);
static int __init bootmode_setup(char *str)
{
	get_option(&str, &bootmode);
	return 1;
}
__setup("bootmode=", bootmode_setup);
#endif

/* Below function is added by synaptics requirement. they want observe
 * the LCD's HSYNC signal in there IC. So we enable the Tout1 pad(on DDI)
 * after power up TSP.
 * I think this is not good solution. because it is not nature, to control
 * LCD's register on TSP driver....
 */
void (*panel_touchkey_on)(void);
void (*panel_touchkey_off)(void);

static void synaptics_enable_sync(bool on)
{
	if (on) {
		if (panel_touchkey_on)
			panel_touchkey_on();
	} else {
		if (panel_touchkey_off)
			panel_touchkey_off();
	}
}

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

		s3c_gpio_cfgpin(GPIO_TSP_nINT_SECURE, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_nINT_SECURE, S3C_GPIO_PULL_NONE);
	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_nINT_SECURE, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_nINT_SECURE, S3C_GPIO_PULL_NONE);
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

/* User firmware */
#define FW_IMAGE_NAME_B0_3_4	"tsp_synaptics/synaptics_b0_3_4.fw"
#define FW_IMAGE_NAME_B0_4_0	"tsp_synaptics/synaptics_b0_4_0.fw"
#define FW_IMAGE_NAME_B0_4_3	"tsp_synaptics/synaptics_b0_4_3.fw"
#define FW_IMAGE_NAME_B0_5_1	"tsp_synaptics/synaptics_b0_5_1.fw"

/* Factory firmware */
#define FAC_FWIMAGE_NAME_B0		"tsp_synaptics/synaptics_b0_fac.fw"
#define FAC_FWIMAGE_NAME_B0_5_1		"tsp_synaptics/synaptics_b0_5_1_fac.fw"
#define NUM_RX	28
#define NUM_TX	16

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.sensor_max_x = 1079,
	.sensor_max_y = 1919,
	.max_touch_width = 28,
	.irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT,/*IRQF_TRIGGER_FALLING,*/
	.power = synaptics_power,
	.gpio = GPIO_TSP_nINT_SECURE,
	.gpio_config = synaptics_gpio_setup,
#ifdef NO_0D_WHILE_2D
	.led_power_on = ts_led_power_on,
	.f1a_button_map = &tm1940_f1a_button_map,
#endif
	.firmware_name = NULL,
	.fac_firmware_name = NULL,
	.num_of_rx = NUM_RX,
	.num_of_tx = NUM_TX,
};

static struct i2c_board_info synaptics_i2c_devs0[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
	}
};

static void synaptics_verify_panel_revision(void)
{
	/* DBh(ID1) of LDI version represents touch IC type and EL type
	 * So we need to verify this infomation(which panel is attatched)
	 * to load proper firmware and display it on factory app.
	 *
	 * 7(Encap) 6~4(Touch ID) 3~2(DCDC) 1~0(EL)
	 *
	 * Touch ID defines
	 * 0 = Atmel mxt540s_revG
	 * 1 = Synaptic IC
	 * 2 = Atmel mxt540s_revS Boot only
	 * 3 = Atmel mxt540s_revS Pre alpha
	 * 4 = Synaptic IC
	 * 5 = Synaptic IC(Remove SUS, connect GND Ring-Guard)
	 * 6 = Synaptic IC(R03 Encap)
	 *
	 * From below IDs, it can be use Hsync signal
	 *
	 * 7 = Synaptic IC(4 level FPCB, R03 Encap + add Level shift TR)
	 * 8 = Synaptic IC(2 level FPCB, R03 Encap + add Level shift TR)
	 *
	 * EL defines
	 * 0 = M4
	 * 1 = M4+
	 */

	u8 el_type = (lcdtype & 0x300) >> 8;
	u8 touch_type = (lcdtype & 0xF000) >> 12;

	/* Definition for firmware name according to panel ID
	 *
	 * ID,	FPCB Name,	User firmware,			Factory firmware
	 * ~ 4,	FPCB 3.4,	FW_IMAGE_NAME_B0_3_4,	FAC_FWIMAGE_NAME_B0
	 * ~ 4,	FPCB 4.0,	FW_IMAGE_NAME_B0_4_0,	FAC_FWIMAGE_NAME_B0
	 * 5,	FPCB 4.3,	FW_IMAGE_NAME_B0_4_3,	FAC_FWIMAGE_NAME_B0
	 * 6,	FPCB 6.0,	FW_IMAGE_NAME_B0_4_3,	FAC_FWIMAGE_NAME_B0
	 * 7,	FPCB 7.x,	FW_IMAGE_NAME_B0_4_3,	FAC_FWIMAGE_NAME_B0
	 * 8,	FPCB 5.x,	FW_IMAGE_NAME_B0_5_1,	FAC_FWIMAGE_NAME_B0_5_1
	 */

	if (touch_type < 5) {
		if (!el_type) {				/* FPCB 3.4 */
			rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_3_4;
		} else {					/* FPCB 4.0 */
			rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_4_0;
		}
		rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0;
	} else if (touch_type == 5) {	/* FPCB 4.3 */
		rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_4_3;
		rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0;
	} else if (touch_type == 6) {	/* FPCB 6.0 */
		rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_4_3;
		rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0;
	} else if (touch_type == 7) {	/* FPCB 7.x */
		rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_4_3;
		rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0;
	} else if (touch_type == 8) {	/* FPCB 8.x 5.x */
		rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_5_1;
		rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0_5_1;
	} else {
		rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_4_3;
		rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0;
	}

	rmi4_platformdata.panel_revision = touch_type;

	/* ID2 of LDI : from 0x25 of ID2, TOUT1 pin is connected
	 * to TSP IC to observe HSYNC signal on TSP IC.
	 * So added enable_sync function added in platform data.
	 */
	rmi4_platformdata.enable_sync = &synaptics_enable_sync;

#ifdef CONFIG_SEC_TSP_FACTORY
	rmi4_platformdata.bootmode = bootmode;
	printk(KERN_ERR "%s: bootmode: %d\n", __func__, bootmode);
#endif

	printk(KERN_ERR "%s: panel_revision: %d, system_rev: %d, lcdtype: 0x%06X, touch_type: %d, el_type: %s, h_sync: %s\n",
		__func__, rmi4_platformdata.panel_revision,	system_rev,
		lcdtype, touch_type, el_type ? "M4+" : "M4",
		!rmi4_platformdata.enable_sync ? "off" : "on");
};

void __init synaptics_tsp_init(void)
{
	gpio_request(GPIO_TSP_nINT_SECURE, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TSP_nINT_SECURE, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_nINT_SECURE, S3C_GPIO_PULL_UP);
	s5p_register_gpio_interrupt(GPIO_TSP_nINT_SECURE);

	rmi4_platformdata.gpio = GPIO_TSP_nINT_SECURE;
	synaptics_i2c_devs0[0].irq = gpio_to_irq(GPIO_TSP_nINT_SECURE);

	synaptics_verify_panel_revision();

	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
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
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = EXYNOS5410_GPX0(2),
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = EXYNOS5410_GPX0(3),
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = EXYNOS5410_GPX0(5),
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
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

static struct platform_device *universal5410_input_devices[] __initdata = {
	&s3c_device_i2c0,
	&universal5410_gpio_keys,
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	&s3c_device_i2c8,
#endif
};

void __init tsp_check_and_init(void)
{
	/* We do not need to check Ic vendor anymore */
	synaptics_tsp_init();
}

void __init exynos5_universal5410_input_init(void)
{
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXTS) || \
	defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
	tsp_check_and_init();
#elif defined(CONFIG_TOUCHSCREEN_MELFAS)
	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
	midas_tsp_init();
#endif
	universal5410_gpio_keys_config_setup();
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	touchkey_init_hw();
	i2c_register_board_info(8, i2c_devs8_emul, ARRAY_SIZE(i2c_devs8_emul));
#endif

#ifdef CONFIG_SENSORS_HALL
	gpio_request(GPIO_HALL_SENSOR_INT, "GPIO_HALL_SENSOR_INT");
	s3c_gpio_cfgpin(GPIO_HALL_SENSOR_INT, S3C_GPIO_SFN(0xf));
	s5p_register_gpio_interrupt(GPIO_HALL_SENSOR_INT);
	gpio_direction_input(GPIO_HALL_SENSOR_INT);
#endif

	platform_add_devices(universal5410_input_devices,
			ARRAY_SIZE(universal5410_input_devices));
}
