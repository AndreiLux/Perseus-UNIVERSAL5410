/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>

#include <plat/gpio-cfg.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-exynos.h>

#include "board-universal5410.h"

#define I2C_BUS_ID_BARCODE	22
#define I2C_BUS_ID_IRLED	23

extern int s3c_gpio_slp_cfgpin(unsigned int pin, unsigned int config);
extern int s3c_gpio_slp_setpull_updown(unsigned int pin, unsigned int config);

extern unsigned int system_rev;

static unsigned int fpga_gpio_table[][4] = {
	{GPIO_IRDA_IRQ,		0, 2, S3C_GPIO_PULL_NONE}, /* IRDA_IRQ */
	{GPIO_BARCODE_SCL_1_8V,	1, 2, S3C_GPIO_PULL_NONE}, /* BARCODE_SCL_1.8v */
	{GPIO_BARCODE_SDA_1_8V,	1, 2, S3C_GPIO_PULL_NONE}, /* BARCODE_SDA_1.8v */
	{GPIO_FPGA_CRESET_B,	1, 2, S3C_GPIO_PULL_NONE}, /* FPGA_CRESET_B */
	{GPIO_FPGA_CDONE,	0, 2, S3C_GPIO_PULL_NONE}, /* FPGA_CDONE */
	{GPIO_FPGA_RST_N,	1, 2, S3C_GPIO_PULL_NONE}, /* FPGA_RST_N */
	{GPIO_FPGA_SPI_SI,	1, 2, S3C_GPIO_PULL_NONE}, /* FPGA_SPI_SI */
	{GPIO_FPGA_SPI_CLK,	1, 2, S3C_GPIO_PULL_NONE}, /* FPGA_SPI_CLK */
	{GPIO_FPGA_SPI_EN,	1, 2, S3C_GPIO_PULL_NONE}, /* FPGA_SPI_EN */
};

void fpga_config_gpio_table(int array_size, unsigned int (*gpio_table)[4])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_setpull(gpio, gpio_table[i][3]);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
		if (gpio_table[i][2] != 2)
			gpio_set_value(gpio, gpio_table[i][2]);
	}
};

static void irda_device_init(void)
{
	int ret;
	printk(KERN_ERR "%s called!\n", __func__);
	ret = gpio_request(GPIO_IRDA_IRQ, "irda_irq");
	if (ret) {
		printk(KERN_ERR "%s: gpio_request fail[%d], ret = %d\n",
			__func__, GPIO_IRDA_IRQ, ret);
		return;
	}
	s3c_gpio_cfgpin(GPIO_IRDA_IRQ, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_IRDA_IRQ, S3C_GPIO_PULL_UP);
	gpio_direction_input(GPIO_IRDA_IRQ);
	printk(KERN_ERR "%s complete\n", __func__);
	return;
};

/* I2C22 */
static struct i2c_gpio_platform_data gpio_i2c_data22 = {
	.scl_pin = GPIO_BARCODE_SCL_1_8V,	/* 36 */
	.sda_pin = GPIO_BARCODE_SDA_1_8V,	/* 37 */
	.udelay = 2,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

struct platform_device s3c_device_i2c22 = {
	.name = "i2c-gpio",
	.id = 22,
	.dev.platform_data = &gpio_i2c_data22,
};

static struct i2c_board_info i2c_devs22_emul[] __initdata = {
	{
		I2C_BOARD_INFO("ice4", (0x6c)),
	},
};

static struct platform_device *universal5410_fpga_devices[] __initdata = {
	&s3c_device_i2c22,
};

void __init exynos5_universal5410_fpga_init(void)
{
	printk(KERN_ERR "[%s] initialization start!\n", __func__);

#if !defined(CONFIG_MACH_JA_KOR_LGT)
	if (system_rev > 5) {
		printk(KERN_ERR "[%s] SYSTEM REVISION is bigger than 5\n", __func__);
		printk(KERN_ERR "[%s] Change the Barcode I2C line\n", __func__);
		fpga_gpio_table[1][0]	= EXYNOS5410_GPH1(4);
		fpga_gpio_table[2][0]	= EXYNOS5410_GPH1(5);
		gpio_i2c_data22.scl_pin = EXYNOS5410_GPH1(4);
		gpio_i2c_data22.sda_pin = EXYNOS5410_GPH1(5);
	}
#else
	if ((system_rev != 7) && (system_rev > 5)) {
		printk(KERN_ERR "[%s] SYSTEM REVISION is bigger than 5\n", __func__);
		printk(KERN_ERR "[%s] Change the Barcode I2C line\n", __func__);
		if(system_rev == 8){
			fpga_gpio_table[1][0]	= EXYNOS5410_GPH1(5);
			fpga_gpio_table[2][0]	= EXYNOS5410_GPH1(6);
			gpio_i2c_data22.scl_pin = EXYNOS5410_GPH1(5);
			gpio_i2c_data22.sda_pin = EXYNOS5410_GPH1(6);
		}else{
			fpga_gpio_table[1][0]	= EXYNOS5410_GPH1(4);
			fpga_gpio_table[2][0]	= EXYNOS5410_GPH1(5);
			gpio_i2c_data22.scl_pin = EXYNOS5410_GPH1(4);
			gpio_i2c_data22.sda_pin = EXYNOS5410_GPH1(5);
		}
	}
#endif
	printk(KERN_ERR "[%s] SYSTEM REVISION : %d\n", __func__, system_rev);
	printk(KERN_ERR "[%s] BARCODE SCL : %d\n", __func__, gpio_i2c_data22.scl_pin);
	printk(KERN_ERR "[%s] BARCODE SDA : %d\n", __func__, gpio_i2c_data22.sda_pin);

	fpga_config_gpio_table(ARRAY_SIZE(fpga_gpio_table),fpga_gpio_table);

	i2c_register_board_info(I2C_BUS_ID_BARCODE, i2c_devs22_emul,
			ARRAY_SIZE(i2c_devs22_emul));

	platform_add_devices(universal5410_fpga_devices,
			ARRAY_SIZE(universal5410_fpga_devices));

	irda_device_init();
};
