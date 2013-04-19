/*
 * sec_gpio.c
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

#include <linux/gpio.h>
#include <linux/serial_core.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-serial.h>
#include <mach/gpio-exynos.h>
#include <plat/cpu.h>
#include <mach/pmu.h>
#include <asm/system_info.h>
#include <mach/sec_gpio.h>

#ifdef CONFIG_ARCH_EXYNOS
#define SEC_GPIO_SET_PD_CFG s5p_gpio_set_pd_cfg
#define SEC_GPIO_SET_PD_PULL s5p_gpio_set_pd_pull
#else
#define SEC_GPIO_SET_PD_CFG
#define SEC_GPIO_SET_PD_PULL
#endif

#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/module.h>

static unsigned int log_en = 0;
module_param_named(log_en, log_en, uint, 0644);

extern unsigned int s3c_gpio_getpin(unsigned int pin);
static void sec_debug_show_sleep_gpio(u32 gpio, u32 cfg)
{
	if (cfg == S3C_GPIO_SLP_PREV) {
		if (!!s3c_gpio_getpin(gpio)) {
			printk(KERN_INFO "gpio-prev:%d=%d\n", gpio, !!s3c_gpio_getpin(gpio));
		}
	}
}

void sec_debug_show_gpio(void)
{
	u32 i;
	u32 cfg, pull, dat;

	for (i = EXYNOS5410_GPJ0(0); i <= EXYNOS5410_GPV4(1); i++) {
		cfg = s3c_gpio_getcfg(i) & 0xF;
		pull = s3c_gpio_getpull(i);
		dat = !!s3c_gpio_getpin(i);

		if (i >= EXYNOS5410_GPX0(0) && i <= EXYNOS5410_GPX3(7)) {
			if (cfg == S3C_GPIO_OUTPUT && dat == 1)
				printk(KERN_INFO "gpio-output:%d=%d\n", i, dat);
		}
		/* Detect abnormal state of gpio */
		if (cfg == S3C_GPIO_INPUT && pull == S3C_GPIO_PULL_DOWN && dat == 1)
			printk(KERN_INFO "gpio-error1:%d=%d\n", i, dat);
		if (cfg == S3C_GPIO_INPUT && pull == S3C_GPIO_PULL_UP && dat == 0)
			printk(KERN_INFO "gpio-error2:%d=%d\n", i, dat);
	}

	if (!log_en)
		return;

	printk(KERN_INFO "====================\n"); {
	char *cfg_str[] = {"IN ", "OUT", "SFN"};
	char *pull_str[] = {"NONE", "DOWN", "RSV ", "UP  "};

	for (i = EXYNOS5410_GPJ0(0); i <= EXYNOS5410_GPV4(1); i++) {
		cfg = s3c_gpio_getcfg(i) & 0xF;
		pull = s3c_gpio_getpull(i);
		dat = !!s3c_gpio_getpin(i);

		printk(KERN_INFO "gpio-%03d:%s-%s-%d", i,
			cfg_str[cfg>2?2:cfg], pull_str[pull>3?2:pull], dat);
	}

	printk(KERN_INFO "====================\n"); }
}
#else
static inline void sec_debug_show_sleep_gpio(u32 gpio, u32 cfg) {}
inline void sec_debug_show_gpio(void) {}
#endif

struct sec_gpio_table_info sec_gpio_table_info;

/* To set gpio configuration for sleep */
static void config_sleep_gpio_table(int array_size,
				    unsigned int (*gpio_table)[3])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		SEC_GPIO_SET_PD_CFG(gpio, gpio_table[i][1]);
		SEC_GPIO_SET_PD_PULL(gpio, gpio_table[i][2]);
		sec_debug_show_sleep_gpio(gpio, gpio_table[i][1]);
	}
}

/*
 * void sec_config_sleep_gpio_table(void)
 * To save power consumption, gpio pin set before enterling sleep
 */
void sec_config_sleep_gpio_table(void)
{
	int i;
	int index;
	struct sec_sleep_table *sleep_table = sec_gpio_table_info.sleep_table;
	unsigned int nr_sleep_table = sec_gpio_table_info.nr_sleep_table;

	index = min(nr_sleep_table, system_rev + 1);

	printk(KERN_DEBUG "%s: Revision=%x\n", __func__, system_rev);

	for (i = 0; i < index; i++) {
		if (sleep_table[i].ptr == NULL)
			continue;

		config_sleep_gpio_table(sleep_table[i].size,
				sleep_table[i].ptr);
	}
}

/*
 * void sec_config_gpio_table(void)
 * Intialize gpio set
 */
void sec_config_gpio_table(void)
{
	u32 i, gpio;
	struct sec_gpio_init_data *init_table = sec_gpio_table_info.init_table;
	unsigned int nr_init_table = sec_gpio_table_info.nr_init_table;

	printk(KERN_INFO "%s: Revision=%x\n", __func__, system_rev);

	for (i = 0; i < nr_init_table; i++) {
		gpio = init_table[i].num;
		if (gpio <= EXYNOS5410_GPV4(1)) {
			s3c_gpio_cfgpin(gpio, init_table[i].cfg);
			s3c_gpio_setpull(gpio, init_table[i].pud);

			if (init_table[i].val != S3C_GPIO_SETPIN_NONE)
				gpio_set_value(gpio, init_table[i].val);

			s5p_gpio_set_drvstr(gpio, init_table[i].drv);
		}
	}
}

/* sec_gpio_init */
int __init sec_gpio_init(void)
{
	int ret = 0;

	/* Add gpio initialize function for each board */
#ifdef CONFIG_MACH_UNIVERSAL5410
	ret = board_universal_5410_init_gpio(&sec_gpio_table_info);
#endif

	return ret;
}

/* arch_initcall_sync(sec_gpio_init); */
