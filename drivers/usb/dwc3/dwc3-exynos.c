/**
 * dwc3-exynos.c - Samsung EXYNOS DWC3 Specific Glue layer
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/platform_data/dwc3-exynos.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>

#include <plat/usb-phy.h>

#include "core.h"

struct dwc3_exynos {
	struct platform_device	*dwc3;
	struct device		*dev;

	struct clk		*clk;
	int			phyclk_gpio;
	int                     vbus_gpio;
};

static int dwc3_setup_vbus_gpio(struct platform_device *pdev)
{
	int err = 0;
	int gpio;

	if (!pdev->dev.of_node)
		return -ENODEV;

	gpio = of_get_named_gpio(pdev->dev.of_node,
				"samsung,vbus-gpio", 0);
	if (!gpio_is_valid(gpio))
		return -ENODEV;

	err = gpio_request(gpio, "dwc3_vbus_gpio");
	if (err) {
		dev_err(&pdev->dev, "can't request dwc3 vbus gpio %d", gpio);
		return err;
	}
	gpio_set_value(gpio, 1);

	return gpio;
}

/*
 * This function gets the GPIO for phyclk from the device tree.
 *
 * If there is nothing in the device tree this returns an error.  If there
 * is something in the device tree it returns that GPIO.
 *
 * NOTE: This also has the side effect of initting the GPIO to enable the
 * phy clock, since that's the default value.
 */
static int dwc3_exynos_phyclk_get_gpio(struct platform_device *pdev)
{
	struct device_node *usbphy;
	int gpio;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	usbphy = of_find_compatible_node(pdev->dev.of_node, NULL,
					 "samsung,exynos-usbphy");
	if (usbphy == NULL)
		ret = -ENODEV;

	gpio = of_get_named_gpio(usbphy, "clock-enable-gpio", 0);
	if (!gpio_is_valid(gpio))
		return -ENODEV;
	dev_dbg(&pdev->dev, "phyclk_gpio = %d\n", gpio);

	ret = gpio_request_one(gpio, GPIOF_INIT_HIGH, "dwc3_phy_clock_en");
	if (ret) {
		dev_err(&pdev->dev, "can't request phyclk gpio %d\n", gpio);
		return ret;
	}
	return gpio;
}

static u64 dwc3_exynos_dma_mask = DMA_BIT_MASK(32);

static int __devinit dwc3_exynos_probe(struct platform_device *pdev)
{
	struct dwc3_exynos_data	*pdata = pdev->dev.platform_data;
	struct platform_device	*dwc3;
	struct dwc3_exynos	*exynos;
	struct clk		*clk;

	int			devid;
	int			ret = -ENOMEM;

	exynos = kzalloc(sizeof(*exynos), GFP_KERNEL);
	if (!exynos) {
		dev_err(&pdev->dev, "not enough memory\n");
		goto err0;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &dwc3_exynos_dma_mask;

	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	exynos->vbus_gpio = dwc3_setup_vbus_gpio(pdev);
	if (!gpio_is_valid(exynos->vbus_gpio))
		dev_warn(&pdev->dev, "Failed to setup vbus gpio\n");

	platform_set_drvdata(pdev, exynos);

	devid = dwc3_get_device_id();
	if (devid < 0)
		goto err1;

	dwc3 = platform_device_alloc("dwc3", devid);
	if (!dwc3) {
		dev_err(&pdev->dev, "couldn't allocate dwc3 device\n");
		goto err2;
	}

	clk = clk_get(&pdev->dev, "usbdrd30");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "couldn't get clock\n");
		ret = -EINVAL;
		goto err3;
	}

	dma_set_coherent_mask(&dwc3->dev, pdev->dev.coherent_dma_mask);

	dwc3->dev.parent = &pdev->dev;
	dwc3->dev.dma_mask = pdev->dev.dma_mask;
	dwc3->dev.dma_parms = pdev->dev.dma_parms;
	exynos->dwc3	= dwc3;
	exynos->dev	= &pdev->dev;
	exynos->clk	= clk;

	clk_enable(exynos->clk);

	/* PHY initialization */
	exynos->phyclk_gpio = dwc3_exynos_phyclk_get_gpio(pdev);
	if (!pdata) {
		dev_dbg(&pdev->dev, "missing platform data\n");
	} else {
		if (pdata->phy_init)
			pdata->phy_init(pdev, pdata->phy_type);
	}

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = platform_device_add_resources(dwc3, pdev->resource,
			pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to dwc3 device\n");
		goto err4;
	}

	ret = platform_device_add(dwc3);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dwc3 device\n");
		goto err4;
	}

	return 0;

err4:
	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, pdata->phy_type);

	clk_disable(clk);
	clk_put(clk);
	pm_runtime_disable(&pdev->dev);
err3:
	platform_device_put(dwc3);
err2:
	dwc3_put_device_id(devid);
err1:
	kfree(exynos);
err0:
	return ret;
}

static int __devexit dwc3_exynos_remove(struct platform_device *pdev)
{
	struct dwc3_exynos	*exynos = platform_get_drvdata(pdev);
	struct dwc3_exynos_data *pdata = pdev->dev.platform_data;

	pm_runtime_disable(&pdev->dev);

	platform_device_unregister(exynos->dwc3);

	dwc3_put_device_id(exynos->dwc3->id);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, pdata->phy_type);

	clk_disable(exynos->clk);
	clk_put(exynos->clk);

	kfree(exynos);

	return 0;
}

#ifdef CONFIG_USB_SUSPEND
static int dwc3_exynos_runtime_suspend(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct platform_device	*pdev = container_of(dev,
						struct platform_device, dev);
	struct dwc3_exynos_data	*pdata = pdev->dev.platform_data;

	dev_dbg(dev, "entering runtime suspend\n");

	if (!pdata) {
		dev_dbg(&pdev->dev, "missing platform data\n");
	} else {
		if (pdata->phyclk_switch)
			pdata->phyclk_switch(pdev, false);
	}

	if (gpio_is_valid(exynos->phyclk_gpio))
		gpio_set_value(exynos->phyclk_gpio, 0);

	return 0;
}

static int dwc3_exynos_runtime_resume(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct platform_device	*pdev = container_of(dev,
						struct platform_device, dev);
	struct dwc3_exynos_data	*pdata = pdev->dev.platform_data;

	dev_dbg(dev, "entering runtime resume\n");

	if (gpio_is_valid(exynos->phyclk_gpio)) {
		gpio_set_value(exynos->phyclk_gpio, 1);

		/*
		 * PI6C557-03 clock generator needs 3ms typically to stabilise,
		 * but the datasheet doesn't list max.  We'll sleep for 10ms
		 * and cross our fingers that it's enough.
		 */
		msleep(10);
	}

	if (!pdata) {
		dev_dbg(&pdev->dev, "missing platform data\n");
	} else {
		if (pdata->phyclk_switch)
			pdata->phyclk_switch(pdev, true);
	}

	return 0;
}
#endif /* CONFIG_USB_SUSPEND */

#ifdef CONFIG_PM_SLEEP
static int dwc3_exynos_suspend(struct device *dev)
{
	struct dwc3_exynos_data	*pdata = dev->platform_data;
	struct dwc3_exynos	*exynos;
	struct platform_device *pdev = to_platform_device(dev);

	exynos = dev_get_drvdata(dev);

	if (!exynos)
		return -EINVAL;

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, pdata->phy_type);

	clk_disable(exynos->clk);

	/* Always force PLL off no matter previous state */
	if (gpio_is_valid(exynos->phyclk_gpio))
		gpio_set_value(exynos->phyclk_gpio, 0);

	if (gpio_is_valid(exynos->vbus_gpio))
		gpio_set_value(exynos->vbus_gpio, 0);

	return 0;
}

static int dwc3_exynos_resume(struct device *dev)
{
	struct dwc3_exynos_data	*pdata = dev->platform_data;
	struct dwc3_exynos	*exynos;
	struct platform_device *pdev = to_platform_device(dev);

	exynos = dev_get_drvdata(dev);

	if (!exynos)
		return -EINVAL;

	if (gpio_is_valid(exynos->vbus_gpio))
		gpio_set_value(exynos->vbus_gpio, 1);

	/*
	 * Init code assumes PLL is on; we'll turn it off again
	 * later if the system decides we want to be runtime
	 * suspended again.
	 */
	if (gpio_is_valid(exynos->phyclk_gpio)) {
		gpio_set_value(exynos->phyclk_gpio, 1);
		/* PI6C557 clock generator needs 3ms to stabilise */
		mdelay(3);
	}

	clk_enable(exynos->clk);

	/* PHY initialization */
	if (!pdata) {
		dev_dbg(&pdev->dev, "missing platform data\n");
	} else {
		if (pdata->phy_init)
			pdata->phy_init(pdev, pdata->phy_type);
	}

	/* runtime set active to reflect active state. */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */


static const struct dev_pm_ops dwc3_exynos_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend		= dwc3_exynos_suspend,
	.resume			= dwc3_exynos_resume,
#endif
#ifdef CONFIG_USB_SUSPEND
	.runtime_suspend	= dwc3_exynos_runtime_suspend,
	.runtime_resume		= dwc3_exynos_runtime_resume,
#endif
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_xhci_match[] = {
	{ .compatible = "samsung,exynos-xhci" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_xhci_match);
#endif

static struct platform_driver dwc3_exynos_driver = {
	.probe		= dwc3_exynos_probe,
	.remove		= __devexit_p(dwc3_exynos_remove),
	.driver		= {
		.name	= "exynos-dwc3",
		.of_match_table = of_match_ptr(exynos_xhci_match),
#ifdef CONFIG_PM
		.pm = &dwc3_exynos_pm_ops,
#endif
	},
};

module_platform_driver(dwc3_exynos_driver);

MODULE_ALIAS("platform:exynos-dwc3");
MODULE_AUTHOR("Anton Tikhomirov <av.tikhomirov@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB3 EXYNOS Glue Layer");
