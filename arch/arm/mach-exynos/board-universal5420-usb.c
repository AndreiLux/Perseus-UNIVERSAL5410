/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/platform_data/dwc3-exynos.h>

#include <plat/ehci.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-fns.h>

#include <mach/usb3-drd.h>
#ifdef CONFIG_V1A
#include <linux/regulator/consumer.h>
#endif
#ifdef CONFIG_USB_GADGET
#include <linux/usb/android_composite.h>
#endif
#include <mach/usb-switch.h>

#if defined(CONFIG_MFD_MAX77803)
#include <linux/mfd/max77803-private.h>
#endif

#if defined(CONFIG_USB_HOST_NOTIFY)
#include <linux/host_notify.h>
#endif

static bool exynos5_usb_vbus_init(struct platform_device *pdev)
{
#if defined(CONFIG_MFD_MAX77803)
	printk(KERN_DEBUG"%s vbus value is (%d) \n",__func__,max77803_muic_read_vbus());
	if(max77803_muic_read_vbus())return 1;
	else return 0;
#else
	return 1;
#endif
}

static int exynos5_usb_get_id_state(struct platform_device *pdev)
{
#if defined(CONFIG_MFD_MAX77803)
	int id;
	id = max77803_muic_read_adc();
	printk(KERN_DEBUG "%s id value is (%d) \n", __func__, id);
	if (id>0)
		return 1;
	else
		return id;
#else
	return 1;
#endif
}

#ifdef CONFIG_USB_GADGET

/* standard android USB platform data */
static struct android_usb_platform_data android_usb_pdata = {
	.nluns = 1,
	.cdfs_support = 0,
};

struct platform_device drd_device_android_usb = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data	= &android_usb_pdata,
	},
};
#endif
static struct s5p_ehci_platdata universal5420_ehci_pdata __initdata;
static struct dwc3_exynos_data universal5420_drd_pdata __initdata = {
	.udc_name		= "exynos-ss-udc",
	.xhci_name		= "exynos-xhci",
	.phy_type		= S5P_USB_PHY_DRD,
	.phy_init		= s5p_usb_phy_init,
	.phy_tune = s5p_usb_phy_tune,
	.phy_exit		= s5p_usb_phy_exit,
	.phy_crport_ctrl	= exynos5_usb_phy_crport_ctrl,
	.get_bses_vld = exynos5_usb_vbus_init,
	.get_id_state = exynos5_usb_get_id_state,
	.irq_flags		= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.id_irq			= -1,
	.vbus_irq		= -1,
};

#ifdef CONFIG_V1A
void usb30_redriver_en(int enable)
{
	struct regulator *regulator;

	if(enable) {
		regulator = regulator_get(NULL, "usb_3.0_drv_3.3v");
		if(IS_ERR(regulator)) {
			pr_err(KERN_ERR "LDO_USB30_REDRIVER_EN set error!\n");
			return;
		}
		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "usb_3.0_drv_3.3v");
		if(IS_ERR(regulator)) {
			pr_err(KERN_ERR "LDO_USB30_REDRIVER_DIS set error!\n");
			return;
		}
		if(regulator_is_enabled(regulator)) {
			regulator_disable(regulator);
		}
		regulator_put(regulator);
	}
}
#else
void usb30_redriver_en(int enable)
{
	int err;

	err = gpio_request(GPIO_USB30_REDRIVER_EN, "GPIO_USB30_REDRIVER_EN");
	if (err) {
		pr_err(KERN_ERR "GPIO_USB30_REDRIVER_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_USB30_REDRIVER_EN, 1);

	gpio_set_value(GPIO_USB30_REDRIVER_EN, enable);
	gpio_free(GPIO_USB30_REDRIVER_EN);

	pr_info("%s: enable = %d\n", __func__, enable);
}
#endif

#if defined(CONFIG_USB_HOST_NOTIFY)
void otg_accessory_power(int enable)
{
	u8 on = (u8)!!enable;

	/* USB3.0 Redriver gpio setting Disable Redriver always works USB2.0
	if(system_rev >= 6)
		usb30_redriver_en(enable);
	*/
	/* max77803 otg power control */
	otg_control(enable);

	pr_info("%s: otg accessory power = %d\n", __func__, on);
}

static void otg_accessory_powered_booster(int enable)
{
	u8 on = (u8)!!enable;

	/* max77803 powered otg power control */
	powered_otg_control(enable);
	pr_info("%s: enable = %d\n", __func__, on);
}

static struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.booster	= otg_accessory_power,
	.powered_booster = otg_accessory_powered_booster,
	.thread_enable	= 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};
#endif

static void __init universal5420_ehci_init(void)
{
	s5p_ehci_set_platdata(&universal5420_ehci_pdata);
}

static void __init universal5420_drd_phy_shutdown(struct platform_device *pdev)
{
	int phy_num = pdev->id;
	struct clk *clk;

	switch (phy_num) {
	case 0:
		clk = clk_get_sys("exynos-dwc3.0", "usbdrd30");
		break;
	case 1:
		clk = clk_get_sys("exynos-dwc3.1", "usbdrd30");
		break;
	default:
		clk = NULL;
		break;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("failed to get DRD%d phy clock\n", phy_num);
		return;
	}

	if (clk_enable(clk)) {
		pr_err("failed to enable DRD%d clock\n", phy_num);
		return;
	}

	s5p_usb_phy_exit(pdev, S5P_USB_PHY_DRD);

	clk_disable(clk);
}

static void __init universal5420_drd0_init(void)
{
	/* initialize DRD0 gpio */
        if (gpio_request(EXYNOS5410_GPK3(0), "UDRD3_0_OVERCUR_U2"))
		printk(KERN_ERR "failed to request UDRD3_0_OVERCUR_U2\n");
        else {
		s3c_gpio_cfgpin(EXYNOS5410_GPK3(0), (0x2 << 0));
		s3c_gpio_setpull(EXYNOS5410_GPK3(0), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK3(0));
	}
	if (gpio_request(EXYNOS5410_GPK3(1), "UDRD3_0_OVERCUR_U3"))
		printk(KERN_ERR "failed to request UDRD3_0_OVERCUR_U3\n");
	else {
		s3c_gpio_cfgpin(EXYNOS5410_GPK3(1), (0x2 << 4));
		s3c_gpio_setpull(EXYNOS5410_GPK3(1), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK3(1));
	}

	if (gpio_request_one(EXYNOS5410_GPK3(2), GPIOF_OUT_INIT_LOW, "UDRD3_0_VBUSCTRL_U2"))
		printk(KERN_ERR "failed to request UDRD3_0_VBUSCTRL_U2\n");
	else {
		s3c_gpio_setpull(EXYNOS5410_GPK3(2), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK3(2));
	}

	if (gpio_request_one(EXYNOS5410_GPK3(3), GPIOF_OUT_INIT_LOW, "UDRD3_0_VBUSCTRL_U3"))
		printk(KERN_ERR "failed to request UDRD3_0_VBUSCTRL_U3\n");
	else {
		s3c_gpio_setpull(EXYNOS5410_GPK3(3), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK3(3));
	}
#if !defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH0)
	universal5420_drd_pdata.quirks = DUMMY_DRD;
#elif defined(CONFIG_USB_EXYNOS_SS_UDC)
	universal5420_drd_pdata.quirks |= FORCE_RUN_PERIPHERAL;
#endif
	universal5420_drd_pdata.quirks |= LOW_VBOOST;

	exynos5_usb3_drd0_set_platdata(&universal5420_drd_pdata);
}

static void __init universal5410_drd1_init(void)
{
	/* Initialize DRD1 gpio */
	if (gpio_request(EXYNOS5410_GPK2(4), "UDRD3_1_OVERCUR_U2"))
		printk(KERN_ERR "failed to request UDRD3_1_OVERCUR_U2\n");
	else {
		s3c_gpio_cfgpin(EXYNOS5410_GPK2(4), (0x2 << 16));
		s3c_gpio_setpull(EXYNOS5410_GPK2(4), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK2(4));
	}

	if (gpio_request(EXYNOS5410_GPK2(5), "UDRD3_1_OVERCUR_U3"))
		printk(KERN_ERR "failed to request UDRD3_1_OVERCUR_U3\n");
	else {
		s3c_gpio_cfgpin(EXYNOS5410_GPK2(5), (0x2 << 20));
		s3c_gpio_setpull(EXYNOS5410_GPK2(5), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK2(5));
	}

	if (gpio_request_one(EXYNOS5410_GPK2(6), GPIOF_OUT_INIT_LOW,
				"UDRD3_1_VBUSCTRL_U2"))
		printk(KERN_ERR "failed to request UDRD3_1_VBUSCTRL_U2\n");
	else {
		s3c_gpio_setpull(EXYNOS5410_GPK2(6), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK2(6));
	}

	if (gpio_request_one(EXYNOS5410_GPK2(7), GPIOF_OUT_INIT_LOW,
				"UDRD3_1_VBUSCTRL_U3"))
		printk(KERN_ERR "failed to request UDRD3_1_VBUSCTRL_U3\n");
	else {
		s3c_gpio_setpull(EXYNOS5410_GPK2(7), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5410_GPK2(7));
	}
#if !defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH1)
	universal5420_drd_pdata.quirks = DUMMY_DRD;
#elif defined(CONFIG_USB_EXYNOS_SS_UDC)
	universal5420_drd_pdata.quirks |= FORCE_RUN_PERIPHERAL;
#endif
	universal5420_drd_pdata.quirks |= LOW_VBOOST;

	exynos5_usb3_drd1_set_platdata(&universal5420_drd_pdata);
}

static struct platform_device *universal5420_usb_devices[] __initdata = {
	&s5p_device_ehci,
	&exynos5_device_usb3_drd0,
	&exynos5_device_usb3_drd1,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&host_notifier_device,
#endif
#ifdef CONFIG_USB_GADGET
	&drd_device_android_usb,
#endif
};
/* USB GADGET */
#ifdef CONFIG_USB_GADGET
void __init exynos5_usbgadget_set_platdata(struct android_usb_platform_data *pd)
{
	s3c_set_platdata(pd, sizeof(struct android_usb_platform_data),
		&drd_device_android_usb);
}

static void __init universal5420_usbgadget_init(void)
{
	struct android_usb_platform_data *android_pdata =
		drd_device_android_usb.dev.platform_data;
	if (android_pdata) {
		unsigned int cdfs = 0;
		unsigned int newluns = 1;
		android_pdata->nluns = newluns;
		android_pdata->cdfs_support = cdfs;
		printk(KERN_DEBUG "usb: %s: default luns=%d, new luns=%d\n",
				__func__, android_pdata->nluns, newluns);

		exynos5_usbgadget_set_platdata(android_pdata);

	} else {
		printk(KERN_DEBUG "usb: %s android_pdata is not available\n",
				__func__);
	}
}
#endif

void __init exynos5_universal5420_usb_init(void)
{
	universal5420_ehci_init();

#ifdef CONFIG_USB_GADGET
	universal5420_usbgadget_init();
#endif


	/*
	 * Shutdown DRD PHYs to reduce power consumption.
	 * Later, DRD driver will turn on only the PHY it needs.
	 */
	universal5420_drd_phy_shutdown(&exynos5_device_usb3_drd0);
	universal5420_drd_phy_shutdown(&exynos5_device_usb3_drd1);
	universal5420_drd0_init();
	universal5410_drd1_init();

	platform_add_devices(universal5420_usb_devices,
			ARRAY_SIZE(universal5420_usb_devices));
}
