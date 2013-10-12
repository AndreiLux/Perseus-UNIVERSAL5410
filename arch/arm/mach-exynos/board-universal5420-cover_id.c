/* board-universal5420-cover_id.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/w1-gpio.h>

#include "board-universal5420.h"

static struct w1_gpio_platform_data w1_gpio_platform_pdata = {
	.pin		= GPIO_COVER_ID,
	.is_open_drain	= 1,
	.slave_speed	= 1,
};

struct platform_device device_cover_id = {
	.name = "w1-gpio",
	.id = -1,
	.dev.platform_data = &w1_gpio_platform_pdata,
};

void __init exynos5_universal5420_cover_id_init(void)
{
	platform_device_register(&device_cover_id);
}
