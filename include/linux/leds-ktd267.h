/*
 * leds-ktd267.h - Flash-led driver for KTD267
 *
 * Copyright (C) 2011 Samsung Electronics
 * DongHyun Chang <dh348.chang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_KTD267_H__
#define __LEDS_KTD267_H__

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>

#define LED_ERROR(x, ...) printk(KERN_ERR "%s : " x, __func__, ##__VA_ARGS__)

enum ktd267_brightness {
	TORCH_BRIGHTNESS_100 = 1,
	TORCH_BRIGHTNESS_89,
	TORCH_BRIGHTNESS_79,
	TORCH_BRIGHTNESS_71,
	TORCH_BRIGHTNESS_63,
	TORCH_BRIGHTNESS_56,
	TORCH_BRIGHTNESS_50,
	TORCH_BRIGHTNESS_45,
	TORCH_BRIGHTNESS_40,
	TORCH_BRIGHTNESS_36,
	TORCH_BRIGHTNESS_32,
	TORCH_BRIGHTNESS_28,
	TORCH_BRIGHTNESS_25,
	TORCH_BRIGHTNESS_22,
	TORCH_BRIGHTNESS_20,
	TORCH_BRIGHTNESS_0,
	TORCH_BRIGHTNESS_INVALID,
};

enum ktd267_status {
	STATUS_UNAVAILABLE = 0,
	STATUS_AVAILABLE,
	STATUS_INVALID,
};

#define IOCTL_KTD267  'A'
#define IOCTL_KTD267_SET_BRIGHTNESS	\
	_IOW(IOCTL_KTD267, 0, enum ktd267_brightness)
#define IOCTL_KTD267_GET_STATUS       \
	_IOR(IOCTL_KTD267, 1, enum ktd267_status)
#define IOCTL_KTD267_SET_POWER        _IOW(IOCTL_KTD267, 2, int)

struct ktd267_led_platform_data {
	enum ktd267_brightness brightness;
	enum ktd267_status status;
	int (*initGpio) (void);
	int (*setGpio) (void);
	int (*freeGpio) (void);
	void (*torch_en) (int onoff);
	void (*torch_set) (int onoff);
};
#endif
