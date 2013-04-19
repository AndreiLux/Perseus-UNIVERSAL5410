/*
 * LED driver for KTD267 - leds-ktd267.c
 *
 * Copyright (C) 2011 DongHyun Chang <dh348.chang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/leds-ktd267.h>
#include <linux/module.h>

static struct ktd267_led_platform_data *led_pdata;

extern struct class *camera_class; /*sys/class/camera*/
static struct device *flash_dev;

static ssize_t ktd267_power(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
    if (buf[0] == '1') {
        led_pdata->setGpio();
        udelay(1);
        led_pdata->torch_set(1);
    } else if (buf[0] == '0') {
        udelay(1);
        led_pdata->torch_set(0);
        led_pdata->freeGpio();
    }
    return count;
}

static DEVICE_ATTR(rear_flash, S_IWUSR|S_IWGRP|S_IROTH,
	NULL, ktd267_power);


static int ktd267_led_probe(struct platform_device *pdev)
{
	LED_ERROR("Probe\n");
	led_pdata =
		(struct ktd267_led_platform_data *) pdev->dev.platform_data;

	flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");
	if (flash_dev < 0)
		pr_err("Failed to create device(flash)!\n");

	if (device_create_file(flash_dev, &dev_attr_rear_flash) < 0) {
		pr_err("failed to create device file, %s\n",
				dev_attr_rear_flash.attr.name);
	}
	
	return 0;
}

static int __devexit ktd267_led_remove(struct platform_device *pdev)
{
	device_remove_file(flash_dev, &dev_attr_rear_flash);
	device_destroy(camera_class, 0);
	class_destroy(camera_class);

	return 0;
}

static struct platform_driver ktd267_led_driver = {
	.probe		= ktd267_led_probe,
	.remove		= __devexit_p(ktd267_led_remove),
	.driver		= {
		.name	= "ktd267-led",
		.owner	= THIS_MODULE,
	},
};

static int __init ktd267_led_init(void)
{
	return platform_driver_register(&ktd267_led_driver);
}

static void __exit ktd267_led_exit(void)
{
	platform_driver_unregister(&ktd267_led_driver);
}

module_init(ktd267_led_init);
module_exit(ktd267_led_exit);

MODULE_AUTHOR("DongHyun Chang <dh348.chang@samsung.com.com>");
MODULE_DESCRIPTION("KTD267 LED driver");
MODULE_LICENSE("GPL");
