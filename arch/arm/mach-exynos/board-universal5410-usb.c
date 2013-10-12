/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/platform_data/dwc3-exynos.h>
#include <plat/ehci.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>
#include <plat/gpio-cfg.h>

#include <mach/ohci.h>
#include <mach/usb3-drd.h>

static struct exynos4_ohci_platdata universal5410_ohci_pdata __initdata;
static struct s5p_ehci_platdata universal5410_ehci_pdata __initdata;
static struct dwc3_exynos_data universal5410_drd_pdata __initdata = {
	.udc_name	= "exynos-ss-udc",
	.xhci_name	= "exynos-xhci",
	.phy_type	= S5P_USB_PHY_DRD,
	.phy_init	= s5p_usb_phy_init,
	.phy_exit	= s5p_usb_phy_exit,
	.phy_crport_ctrl = exynos5_usb_phy_crport_ctrl,
	.id_irq		= -1,
	.vbus_irq	= -1,
};


static void __init universal5410_ohci_init(void)
{
	exynos4_ohci_set_platdata(&universal5410_ohci_pdata);
}

static void __init universal5410_ehci_init(void)
{
	s5p_ehci_set_platdata(&universal5410_ehci_pdata);
}

static void __init universal5410_drd_phy_shutdown(struct platform_device *pdev)
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
		printk(KERN_ERR "failed to get DRD%d phy clock\n", phy_num);
		return;
	}

	if (clk_enable(clk)) {
		printk(KERN_ERR "failed to enable DRD%d clock\n", phy_num);
		return;
	}

	s5p_usb_phy_exit(pdev, S5P_USB_PHY_DRD);

	clk_disable(clk);
}

static void __init universal5410_drd0_init(void)
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

	/*
	 * Activate USB Device Controller (UDC) during booting.
	 *
	 * IMPORTANT: this solution is temporal. Use
	 * exynos_drd_switch_id_event() and exynos_drd_switch_vbus_event()
	 * callbacks to switch DRD role at runtime.
	 */
#if defined(CONFIG_USB_EXYNOS_SS_UDC)
	universal5410_drd_pdata.quirks |= FORCE_RUN_PERIPHERAL |
					  FORCE_INIT_PERIPHERAL |
					  FORCE_PM_PERIPHERAL;
#endif

	exynos5_usb3_drd0_set_platdata(&universal5410_drd_pdata);
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
#if defined(CONFIG_USB_EXYNOS_SS_UDC)
	universal5410_drd_pdata.quirks |= FORCE_RUN_PERIPHERAL |
					  FORCE_INIT_PERIPHERAL |
					  FORCE_PM_PERIPHERAL;
#endif
	exynos5_usb3_drd1_set_platdata(&universal5410_drd_pdata);
}

static struct platform_device *universal5410_usb_devices[] __initdata = {
	&exynos4_device_ohci,
	&s5p_device_ehci,
#if defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH0)
	&exynos5_device_usb3_drd0,
#else
	&exynos5_device_usb3_drd1,
#endif
};

void __init exynos5_universal5410_usb_init(void)
{
	universal5410_ohci_init();
	universal5410_ehci_init();

	/*
	 * Shutdown DRD PHYs to reduce power consumption.
	 * Later, DRD driver will turn on only the PHY it needs.
	 */
	universal5410_drd_phy_shutdown(&exynos5_device_usb3_drd0);
	universal5410_drd_phy_shutdown(&exynos5_device_usb3_drd1);

	universal5410_drd0_init();
	universal5410_drd1_init();

	platform_add_devices(universal5410_usb_devices,
			ARRAY_SIZE(universal5410_usb_devices));
}
