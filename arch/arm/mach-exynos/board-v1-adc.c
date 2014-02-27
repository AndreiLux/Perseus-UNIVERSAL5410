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
#include <linux/platform_data/stmpe811-adc.h>
#include <linux/i2c-gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/devs.h>

#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/irqs.h>

#ifdef CONFIG_30PIN_CONN
#include <linux/30pin_con.h>
#endif

#ifdef CONFIG_SAMSUNG_MHL_8240
#include <linux/sii8240.h>
#endif

#include "board-universal5410.h"
#if defined(CONFIG_USB_HOST_NOTIFY) && defined(CONFIG_30PIN_CONN)
#include <mach/usb3-drd.h>
#include <linux/host_notify.h>
#endif

#ifdef CONFIG_30PIN_CONN
void v1_accessory_power(u8 token, bool active)
{
	int try_cnt = 0;
	int ret ;
	static u8 acc_en_token;

	/* token info */
	/* forced power off = 0, Keyboard dock = 1, USB HOST = 2,
	MHL dongle = 3 */
	if (active) {
		if ((token==2) && (acc_en_token)) { /*USB PRINT CASE*/
			pr_info("Board : Keyboard dock is connected.\n");
			gpio_set_value(GPIO_ACCESSORY_EN,0);
			msleep(100);
		}

		acc_en_token |= (1 << token);

		/* prevent the overcurrent */
		while (!gpio_get_value(GPIO_ACCESSORY_CHECK)) {
			gpio_set_value(GPIO_ACCESSORY_EN,0);
			usleep_range(20000, 20000);
			gpio_set_value(GPIO_ACCESSORY_EN,1);
			usleep_range(20000, 20000);
			if (try_cnt > 10) {
				break;
			} else
				try_cnt++;
		}
		
		ret = gpio_get_value(GPIO_ACCESSORY_CHECK);
		pr_info("%s ON by (token: %d) result: %s, try_count:%d\n",
			__func__,	token, ret?"SUCCESS":"FAILED",try_cnt);

	} else {
		if (0 == token) { /* Forced power down */
			gpio_set_value(GPIO_ACCESSORY_EN,0);
		} else {
			acc_en_token &= ~(1 << token);
			if (0 == acc_en_token) {
				gpio_set_value(GPIO_ACCESSORY_EN,0);
			}
		}
		ret = gpio_get_value(GPIO_ACCESSORY_CHECK);
		pr_info("%s OFF by (token: %d) result: %s, try_count:%d\n", __func__, token,
			ret ? "FAILED":"SUCCESS",try_cnt);
	}
}

#ifdef CONFIG_SEC_KEYBOARD_DOCK
static struct sec_keyboard_callbacks *keyboard_callbacks;
static int check_sec_keyboard_dock(bool attached)
{
	if (keyboard_callbacks && keyboard_callbacks->check_keyboard_dock)
		return keyboard_callbacks->
			check_keyboard_dock(keyboard_callbacks, attached);
	return 0;
}

/* call 30pin func. from sec_keyboard */
static struct sec_30pin_callbacks *s30pin_callbacks;
static int noti_sec_univ_kbd_dock(unsigned int code)
{
	if (s30pin_callbacks && s30pin_callbacks->noti_univ_kdb_dock)
		return s30pin_callbacks->
			noti_univ_kdb_dock(s30pin_callbacks, code);
	return 0;
}

static void v1_keyboard_uart_path(bool en)
{
	if (en) {
		gpio_direction_output(GPIO_UART_SEL1, 1);
		gpio_direction_output(GPIO_UART_SEL2, 0);
		printk(KERN_DEBUG "[Keyboard] uart_sel : 1, 0\n");
	} else {
		gpio_direction_output(GPIO_UART_SEL1, 1);
		gpio_direction_output(GPIO_UART_SEL2, 1);
		printk(KERN_DEBUG "[Keyboard] uart_sel : 1, 1\n");
	}
}

static void sec_30pin_register_cb(struct sec_30pin_callbacks *cb)
{
	s30pin_callbacks = cb;
}

static void sec_keyboard_register_cb(struct sec_keyboard_callbacks *cb)
{
	keyboard_callbacks = cb;
}

static struct sec_keyboard_platform_data acc_30pincon_kbd_pdata = {
	.dock_irq_gpio = GPIO_DOCK_INT,
	.acc_power = v1_accessory_power,
	.check_uart_path = v1_keyboard_uart_path,
	.register_cb = sec_keyboard_register_cb,
	.noti_univ_kbd_dock = noti_sec_univ_kbd_dock,
	.wakeup_key = NULL,
};

static struct platform_device acc_30pin_keyboard_dock = {
	.name	= "sec_keyboard",
	.id	= -1,
	.dev = {
		.platform_data = &acc_30pincon_kbd_pdata,
	}
};
#endif

int v1_accessory_adc(void)
{
	int ret = 0;

	ret = stmpe811_read_adc_data(7);
	printk("[ACC] v1_accessory_adc 7 ret = %d", ret);

	return ret ;
}

static int v1_get_acc_state(void)
{
	return gpio_get_value(GPIO_ACCESSORY_INT);
}

static int v1_get_dock_state(void)
{
	return gpio_get_value(GPIO_DOCK_INT);
}
#if defined(CONFIG_USB_HOST_NOTIFY) && defined(CONFIG_30PIN_CONN)
static void v1_usb_otg_power(int active);
#define HOST_NOTIFIER_BOOSTER	v1_usb_otg_power
#define HOST_NOTIFIER_GPIO		GPIO_ACCESSORY_CHECK
#define RETRY_CNT_LIMIT 100

struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.gpio		= HOST_NOTIFIER_GPIO,
	.booster	= HOST_NOTIFIER_BOOSTER,
	.irq_enable = 1,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void v1_usb_otg_power(int active)
{
	v1_accessory_power(2, active);
}

static void v1_check_id_state(int state)
{
	pr_info("%s: id state = %d\n", __func__, state);
#if defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH0)
	exynos_drd_switch_id_event(&exynos5_device_usb3_drd0, state);
#else
	exynos_drd_switch_id_event(&exynos5_device_usb3_drd1, state);
#endif
}

static void v1_usb_otg_en(int active)
{
	pr_info("otg %s : %d\n", __func__, active);

	if (active) { /* USB Host enable */
		v1_check_id_state(0);
		v1_usb_otg_power(1); /* vbus activate */
		host_notifier_pdata.ndev.mode = NOTIFY_HOST_MODE;
		if (host_notifier_pdata.usbhostd_start)
			host_notifier_pdata.usbhostd_start();
	} else { /* USB Host disable */
		v1_check_id_state(1);
		v1_usb_otg_power(0); /* vbus deactivate */
		host_notifier_pdata.ndev.mode = NOTIFY_NONE_MODE;
		if (host_notifier_pdata.usbhostd_stop)
			host_notifier_pdata.usbhostd_stop();
	}
}
#endif
struct acc_con_platform_data acc_30pincon_pdata = {
#if defined(CONFIG_USB_HOST_NOTIFY) && defined(CONFIG_30PIN_CONN)
	.otg_en = v1_usb_otg_en,
#endif
	.acc_power = v1_accessory_power,
	.usb_ldo_en = NULL,
	.get_acc_state = v1_get_acc_state,
	.get_dock_state = v1_get_dock_state,
#ifdef CONFIG_SEC_KEYBOARD_DOCK
	.check_keyboard = check_sec_keyboard_dock,
#endif
	.get_accessory_id = v1_accessory_adc,
/*	.register_cb = sec_30pin_register_cb,*/
	.accessory_irq_gpio = GPIO_ACCESSORY_INT,
	.dock_irq_gpio = GPIO_DOCK_INT,
#if defined(CONFIG_SAMSUNG_MHL_8240)
	.acc_mhl_noti_cb = acc_notify,
#endif
};

struct platform_device acc_30pin_connector = {
	.name = "acc_con",
	.id = -1,
	.dev.platform_data = &acc_30pincon_pdata,
};

static struct platform_device *universal5410_accessory_devices[] __initdata = {
	&acc_30pin_connector,
#ifdef CONFIG_SEC_KEYBOARD_DOCK
	&acc_30pin_keyboard_dock,
#endif
#if defined(CONFIG_USB_HOST_NOTIFY) && defined(CONFIG_30PIN_CONN)
	&host_notifier_device,
#endif
};

void exynos5_universal5410_accessory_init(void)
{

	gpio_request(GPIO_ACCESSORY_INT, "accessory");
	s3c_gpio_cfgpin(GPIO_ACCESSORY_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_ACCESSORY_INT, S3C_GPIO_PULL_NONE);
	gpio_direction_input(GPIO_ACCESSORY_INT);

	gpio_request(GPIO_DOCK_INT, "dock");
	s3c_gpio_cfgpin(GPIO_DOCK_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_DOCK_INT, S3C_GPIO_PULL_NONE);
	gpio_direction_input(GPIO_DOCK_INT);

	gpio_request(GPIO_ACCESSORY_EN, "GPIO_ACCESSORY_EN");
	s3c_gpio_cfgpin(GPIO_ACCESSORY_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_ACCESSORY_EN, S3C_GPIO_PULL_NONE);

	gpio_request(GPIO_ACCESSORY_CHECK, "GPIO_ACCESSORY_CHECK");
	s5p_register_gpio_interrupt(GPIO_ACCESSORY_CHECK);
	s3c_gpio_cfgpin(GPIO_ACCESSORY_CHECK, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_ACCESSORY_CHECK, S3C_GPIO_PULL_NONE);

	platform_add_devices(universal5410_accessory_devices,
		ARRAY_SIZE(universal5410_accessory_devices));
}

#endif


static struct stmpe811_callbacks *stmpe811_cbs;
static void stmpe811_register_callback(struct stmpe811_callbacks *cb)
{
	stmpe811_cbs = cb;
}

int stmpe811_read_adc_data(u8 channel)
{
	if (stmpe811_cbs && stmpe811_cbs->get_adc_data)
		return stmpe811_cbs->get_adc_data(channel);

	return -EINVAL;
}

struct stmpe811_platform_data stmpe811_pdata = {
	.register_cb = stmpe811_register_callback,
};

/* I2C19 */
static struct i2c_board_info i2c_devs19[] __initdata = {
	{
		I2C_BOARD_INFO("stmpe811-adc", (0x82 >> 1)),
		.platform_data  = &stmpe811_pdata,
	},
};

static struct i2c_gpio_platform_data gpio_i2c_data19 = {
	.scl_pin = GPIO_ADC_I2C_SCL,
	.sda_pin = GPIO_ADC_I2C_SDA,
	.udelay = 2,		/* 250KHz */
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

struct platform_device s3c_device_i2c19 = {
	.name = "i2c-gpio",
	.id = 19,
	.dev.platform_data = &gpio_i2c_data19,
};

static struct platform_device *universal5410_adc_devices[] __initdata = {
	&s3c_device_i2c19,
};


void __init exynos5_universal5410_stmpe_adc_init(void)
{
	pr_err("%s:\n", __func__);

	i2c_register_board_info(19, i2c_devs19,
			ARRAY_SIZE(i2c_devs19));

	platform_add_devices(universal5410_adc_devices,
			ARRAY_SIZE(universal5410_adc_devices));
}

