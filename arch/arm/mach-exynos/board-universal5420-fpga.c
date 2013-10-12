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

#include "board-universal5420.h"

#define I2C_BUS_ID_BARCODE	22
#define I2C_BUS_ID_IRLED	23

extern int s3c_gpio_slp_cfgpin(unsigned int pin, unsigned int config);
extern int s3c_gpio_slp_setpull_updown(unsigned int pin, unsigned int config);

extern unsigned int system_rev;

#if defined(USING_SPI_AND_I2C_IN_SAME_LINE)
extern void barcode_fpga_firmware_update(void);
#endif

static unsigned int fpga_gpio_table[][4] = {
	{GPIO_IRDA_IRQ,		0, 2, S3C_GPIO_PULL_NONE}, /* IRDA_IRQ */
// REV0.4 no BARCODE I2C
#if !defined(USING_SPI_AND_I2C_IN_SAME_LINE)
	{GPIO_BARCODE_SCL_1_8V,	1, 1, S3C_GPIO_PULL_NONE}, /* BARCODE_SCL_1.8v */
	{GPIO_BARCODE_SDA_1_8V,	1, 1, S3C_GPIO_PULL_NONE}, /* BARCODE_SDA_1.8v */
#endif
	{GPIO_FPGA_CRESET_B,	1, 1, S3C_GPIO_PULL_NONE}, /* FPGA_CRESET_B */
	{GPIO_FPGA_CDONE,	0, 2, S3C_GPIO_PULL_NONE}, /* FPGA_CDONE */
	{GPIO_FPGA_RST_N,	1, 0, S3C_GPIO_PULL_NONE}, /* FPGA_RST_N */
	{GPIO_FPGA_SPI_SI,	1, 0, S3C_GPIO_PULL_NONE}, /* FPGA_SPI_SI */
	{GPIO_FPGA_SPI_CLK,	1, 0, S3C_GPIO_PULL_NONE}, /* FPGA_SPI_CLK */
	{GPIO_FPGA_SPI_EN,	1, 0, S3C_GPIO_PULL_NONE}, /* FPGA_SPI_EN */
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
	s3c_gpio_setpull(GPIO_IRDA_IRQ, S3C_GPIO_PULL_NONE);
	gpio_direction_input(GPIO_IRDA_IRQ);
	printk(KERN_ERR "%s complete\n", __func__);
	return;
};

/* I2C22 */
static struct i2c_gpio_platform_data gpio_i2c_data22 = {
// REV0.4 no BARCODE I2C
#if !defined(USING_SPI_AND_I2C_IN_SAME_LINE)
	.scl_pin = GPIO_BARCODE_SCL_1_8V,	/* 36 */
	.sda_pin = GPIO_BARCODE_SDA_1_8V,	/* 37 */
#else
	.scl_pin = GPIO_FPGA_SPI_CLK,
	.sda_pin = GPIO_FPGA_SPI_SI,
#endif
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

static struct platform_device *universal5420_fpga_devices[] __initdata = {
	&s3c_device_i2c22,
};

void __init exynos5_universal5420_fpga_init(void)
{
	printk(KERN_ERR "[%s] initialization start!\n", __func__);
#if defined(USING_SPI_AND_I2C_IN_SAME_LINE)
	barcode_fpga_firmware_update();
#endif

	fpga_config_gpio_table(ARRAY_SIZE(fpga_gpio_table),fpga_gpio_table);

	i2c_register_board_info(I2C_BUS_ID_BARCODE, i2c_devs22_emul,
			ARRAY_SIZE(i2c_devs22_emul));

	platform_add_devices(universal5420_fpga_devices,
			ARRAY_SIZE(universal5420_fpga_devices));

	irda_device_init();
};
