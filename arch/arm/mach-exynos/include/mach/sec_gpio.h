/*
 * sec_gpio.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - SEC GPIO common
 * Author: Chiwoong Byun <woong.byun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_GPIO_H
#define __SEC_GPIO_H __FILE__

#include <linux/gpio.h>
#include <linux/serial_core.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-serial.h>
#include <mach/gpio-exynos.h>
#include <plat/cpu.h>
#include <mach/pmu.h>
#include <asm/system_info.h>

#define S3C_GPIO_SLP_OUT0       (0x00)
#define S3C_GPIO_SLP_OUT1       (0x01)
#define S3C_GPIO_SLP_INPUT      (0x02)
#define S3C_GPIO_SLP_PREV       (0x03)

#define S3C_GPIO_SETPIN_ZERO         0
#define S3C_GPIO_SETPIN_ONE          1
#define S3C_GPIO_SETPIN_NONE         2

#define GPIO_TABLE(_ptr) \
	{.ptr = _ptr, \
	.size = ARRAY_SIZE(_ptr)} \

 #define GPIO_TABLE_NULL \
	{.ptr = NULL, \
	.size = 0} \

struct sec_gpio_init_data {
	uint num;
	uint cfg;
	uint val;
	uint pud;
	uint drv;
};

struct sec_sleep_table {
	unsigned int (*ptr)[3];
	int size;
};

struct sec_gpio_table_info {
	struct sec_gpio_init_data *init_table;
	unsigned int nr_init_table;
	struct sec_sleep_table *sleep_table;
	unsigned int nr_sleep_table;
};

#ifdef CONFIG_MACH_UNIVERSAL5410
extern int board_universal_5410_init_gpio(struct sec_gpio_table_info *sec_gpio_table_info);
#endif

#endif /* __SEC_GPIO_H */
