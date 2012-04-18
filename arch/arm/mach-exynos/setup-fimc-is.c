/* linux/arch/arm/mach-exynos/setup-fimc-is.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <media/exynos_fimc_is.h>
#include <linux/regulator/consumer.h>

struct platform_device; /* don't need the contents */

/*------------------------------------------------------*/
/*		Exynos5 series - FIMC-IS		*/
/*------------------------------------------------------*/
void exynos5_fimc_is_cfg_gpio(struct platform_device *pdev)
{
	int ret;
	int i;
	struct exynos5_platform_fimc_is *dev = pdev->dev.platform_data;
	struct gpio_set *gpio;

	for (i = 0; i < FIMC_IS_MAX_GPIO_NUM; i++) {
		if (!dev->gpio_info->gpio[i].pin)
			continue;

		gpio = &dev->gpio_info->gpio[i];

		ret = gpio_request(gpio->pin, gpio->name);
		if (ret)
			printk(KERN_ERR "Request GPIO error(%s)\n", gpio->name);

		switch (gpio->act) {
		case GPIO_PULL_NONE:
			s3c_gpio_cfgpin(gpio->pin, gpio->value);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			break;
		case GPIO_OUTPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_RESET:
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio->pin, 0);
			gpio_direction_output(gpio->pin, 1);
			break;
		default:
			printk(KERN_ERR "unknown act for gpio\n");
		}
		gpio_free(gpio->pin);
	}
}


int exynos5_fimc_is_cfg_clk(struct platform_device *pdev)
{
	struct clk *aclk_mcuisp = NULL;
	struct clk *aclk_266 = NULL;
	struct clk *aclk_mcuisp_div0 = NULL;
	struct clk *aclk_mcuisp_div1 = NULL;
	struct clk *aclk_266_div0 = NULL;
	struct clk *aclk_266_div1 = NULL;
	struct clk *aclk_266_mpwm = NULL;
	struct clk *sclk_uart_isp = NULL;
	struct clk *sclk_uart_isp_div = NULL;
	struct clk *mout_mpll = NULL;
	struct clk *sclk_mipi = NULL;
	struct clk *cam_src = NULL;
	struct clk *cam_A_clk = NULL;
	unsigned long mcu_isp_400;
	unsigned long isp_266;
	unsigned long isp_uart;
	unsigned long mipi;
	unsigned long epll;

	/* 1. MCUISP */
	aclk_mcuisp = clk_get(&pdev->dev, "aclk_400_isp");
	if (IS_ERR(aclk_mcuisp))
		return PTR_ERR(aclk_mcuisp);

	aclk_mcuisp_div0 = clk_get(&pdev->dev, "aclk_400_isp_div0");
	if (IS_ERR(aclk_mcuisp_div0))
		return PTR_ERR(aclk_mcuisp_div0);

	aclk_mcuisp_div1 = clk_get(&pdev->dev, "aclk_400_isp_div1");
	if (IS_ERR(aclk_mcuisp_div1))
		return PTR_ERR(aclk_mcuisp_div1);

	clk_set_rate(aclk_mcuisp_div0, 400 * 1000000);
	clk_set_rate(aclk_mcuisp_div1, 400 * 1000000);

	mcu_isp_400 = clk_get_rate(aclk_mcuisp);
	printk(KERN_DEBUG "mcu_isp_400 : %ld\n", mcu_isp_400);

	mcu_isp_400 = clk_get_rate(aclk_mcuisp_div0);
	printk(KERN_DEBUG "mcu_isp_400_div0 : %ld\n", mcu_isp_400);

	mcu_isp_400 = clk_get_rate(aclk_mcuisp_div1);
	printk(KERN_DEBUG "aclk_mcuisp_div1 : %ld\n", mcu_isp_400);

	clk_put(aclk_mcuisp);
	clk_put(aclk_mcuisp_div0);
	clk_put(aclk_mcuisp_div1);

	/* 2. ACLK_ISP */
	aclk_266 = clk_get(&pdev->dev, "aclk_266_isp");
	if (IS_ERR(aclk_266))
		return PTR_ERR(aclk_266);
	aclk_266_div0 = clk_get(&pdev->dev, "aclk_266_isp_div0");
	if (IS_ERR(aclk_266_div0))
		return PTR_ERR(aclk_266_div0);
	aclk_266_div1 = clk_get(&pdev->dev, "aclk_266_isp_div1");
	if (IS_ERR(aclk_266_div1))
		return PTR_ERR(aclk_266_div1);
	aclk_266_mpwm = clk_get(&pdev->dev, "aclk_266_isp_divmpwm");
	if (IS_ERR(aclk_266_mpwm))
		return PTR_ERR(aclk_266_mpwm);

	clk_set_rate(aclk_266_div0, 134 * 1000000);
	clk_set_rate(aclk_266_div1, 68 * 1000000);

	isp_266 = clk_get_rate(aclk_266);
	printk(KERN_DEBUG "isp_266 : %ld\n", isp_266);

	isp_266 = clk_get_rate(aclk_266_div0);
	printk(KERN_DEBUG "isp_266_div0 : %ld\n", isp_266);

	isp_266 = clk_get_rate(aclk_266_div1);
	printk(KERN_DEBUG "isp_266_div1 : %ld\n", isp_266);

	isp_266 = clk_get_rate(aclk_266_mpwm);
	printk(KERN_DEBUG "isp_266_mpwm : %ld\n", isp_266);

	clk_put(aclk_266);
	clk_put(aclk_266_div0);
	clk_put(aclk_266_div1);
	clk_put(aclk_266_mpwm);

	/* 3. UART-ISP */
	sclk_uart_isp = clk_get(&pdev->dev, "sclk_uart_src_isp");
	if (IS_ERR(sclk_uart_isp))
		return PTR_ERR(sclk_uart_isp);

	sclk_uart_isp_div = clk_get(&pdev->dev, "sclk_uart_isp");
	if (IS_ERR(sclk_uart_isp_div))
		return PTR_ERR(sclk_uart_isp_div);

	clk_set_parent(sclk_uart_isp_div, sclk_uart_isp);
	clk_set_rate(sclk_uart_isp_div, 50 * 1000000);

	isp_uart = clk_get_rate(sclk_uart_isp);
	printk(KERN_DEBUG "isp_uart : %ld\n", isp_uart);
	isp_uart = clk_get_rate(sclk_uart_isp_div);
	printk(KERN_DEBUG "isp_uart_div : %ld\n", isp_uart);

	clk_put(sclk_uart_isp);
	clk_put(sclk_uart_isp_div);

	/* 4. MIPI-CSI */
	mout_mpll = clk_get(&pdev->dev, "mout_mpll_user");
	if (IS_ERR(mout_mpll))
		return PTR_ERR(mout_mpll);
	sclk_mipi = clk_get(&pdev->dev, "sclk_gscl_wrap0");
	if (IS_ERR(sclk_mipi))
		return PTR_ERR(sclk_mipi);

	clk_set_parent(sclk_mipi, mout_mpll);
	clk_set_rate(sclk_mipi, 267 * 1000000);


	mout_mpll = clk_get(&pdev->dev, "mout_mpll_user");
	if (IS_ERR(mout_mpll))
		return PTR_ERR(mout_mpll);
	sclk_mipi = clk_get(&pdev->dev, "sclk_gscl_wrap1");
	if (IS_ERR(sclk_mipi))
		return PTR_ERR(sclk_mipi);

	clk_set_parent(sclk_mipi, mout_mpll);
	clk_set_rate(sclk_mipi, 267 * 1000000);
	mipi = clk_get_rate(mout_mpll);
	printk(KERN_DEBUG "mipi_src : %ld\n", mipi);
	mipi = clk_get_rate(sclk_mipi);
	printk(KERN_DEBUG "mipi_div : %ld\n", mipi);

	clk_put(mout_mpll);
	clk_put(sclk_mipi);

	/* 5. Camera A */
	cam_src = clk_get(&pdev->dev, "xxti");
	if (IS_ERR(cam_src))
		return PTR_ERR(cam_src);
	cam_A_clk = clk_get(&pdev->dev, "sclk_cam0");
	if (IS_ERR(cam_A_clk))
		return PTR_ERR(cam_A_clk);

	epll = clk_get_rate(cam_src);
	printk(KERN_DEBUG "epll : %ld\n", epll);

	clk_set_parent(cam_A_clk, cam_src);
	clk_set_rate(cam_A_clk, 24 * 1000000);

	clk_put(cam_src);
	clk_put(cam_A_clk);

	/* 6. Camera B */
	cam_src = clk_get(&pdev->dev, "xxti");
	if (IS_ERR(cam_src))
		return PTR_ERR(cam_src);
	cam_A_clk = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(cam_A_clk))
		return PTR_ERR(cam_A_clk);

	epll = clk_get_rate(cam_src);
	printk(KERN_DEBUG "epll : %ld\n", epll);

	clk_set_parent(cam_A_clk, cam_src);
	clk_set_rate(cam_A_clk, 24 * 1000000);

	clk_put(cam_src);
	clk_put(cam_A_clk);

	return 0;
}

int exynos5_fimc_is_clk_on(struct platform_device *pdev)
{
	struct clk *gsc_ctrl = NULL;
	struct clk *isp_ctrl = NULL;
	struct clk *mipi_ctrl = NULL;
	struct clk *cam_if_top = NULL;
	struct clk *cam_A_clk = NULL;

	gsc_ctrl = clk_get(&pdev->dev, "gscl");
	if (IS_ERR(gsc_ctrl))
		return PTR_ERR(gsc_ctrl);

	clk_enable(gsc_ctrl);
	clk_put(gsc_ctrl);

	isp_ctrl = clk_get(&pdev->dev, "isp0");
	if (IS_ERR(isp_ctrl))
		return PTR_ERR(isp_ctrl);

	clk_enable(isp_ctrl);
	clk_put(isp_ctrl);

	isp_ctrl = clk_get(&pdev->dev, "isp1");
	if (IS_ERR(isp_ctrl))
		return PTR_ERR(isp_ctrl);

	clk_enable(isp_ctrl);
	clk_put(isp_ctrl);

	mipi_ctrl = clk_get(&pdev->dev, "gscl_wrap0");
	if (IS_ERR(mipi_ctrl))
		return PTR_ERR(mipi_ctrl);

	clk_enable(mipi_ctrl);
	clk_put(mipi_ctrl);

	mipi_ctrl = clk_get(&pdev->dev, "gscl_wrap1");
	if (IS_ERR(mipi_ctrl))
		return PTR_ERR(mipi_ctrl);

	clk_enable(mipi_ctrl);
	clk_put(mipi_ctrl);

	cam_if_top = clk_get(&pdev->dev, "camif_top");
	if (IS_ERR(cam_if_top))
		return PTR_ERR(cam_if_top);

	clk_enable(cam_if_top);
	clk_put(cam_if_top);

	cam_A_clk = clk_get(&pdev->dev, "sclk_cam0");
	if (IS_ERR(cam_A_clk))
		return PTR_ERR(cam_A_clk);

	clk_enable(cam_A_clk);
	clk_put(cam_A_clk);

	cam_A_clk = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(cam_A_clk))
		return PTR_ERR(cam_A_clk);

	clk_enable(cam_A_clk);
	clk_put(cam_A_clk);

	return 0;
}

int exynos5_fimc_is_clk_off(struct platform_device *pdev)
{
	struct clk *gsc_ctrl = NULL;
	struct clk *isp_ctrl = NULL;
	struct clk *mipi_ctrl = NULL;
	struct clk *cam_if_top = NULL;
	struct clk *cam_A_clk = NULL;

	gsc_ctrl = clk_get(&pdev->dev, "gscl");
	if (IS_ERR(gsc_ctrl))
		return PTR_ERR(gsc_ctrl);

	clk_disable(gsc_ctrl);
	clk_put(gsc_ctrl);

	isp_ctrl = clk_get(&pdev->dev, "isp0");
	if (IS_ERR(isp_ctrl))
		return PTR_ERR(isp_ctrl);

	clk_disable(isp_ctrl);
	clk_put(isp_ctrl);

	isp_ctrl = clk_get(&pdev->dev, "isp1");
	if (IS_ERR(isp_ctrl))
		return PTR_ERR(isp_ctrl);

	clk_disable(isp_ctrl);
	clk_put(isp_ctrl);

	mipi_ctrl = clk_get(&pdev->dev, "gscl_wrap0");
	if (IS_ERR(mipi_ctrl))
		return PTR_ERR(mipi_ctrl);

	clk_disable(mipi_ctrl);
	clk_put(mipi_ctrl);


	mipi_ctrl = clk_get(&pdev->dev, "gscl_wrap1");
	if (IS_ERR(mipi_ctrl))
		return PTR_ERR(mipi_ctrl);

	clk_disable(mipi_ctrl);
	clk_put(mipi_ctrl);


	cam_if_top = clk_get(&pdev->dev, "camif_top");
	if (IS_ERR(cam_if_top))
		return PTR_ERR(cam_if_top);

	clk_disable(cam_if_top);
	clk_put(cam_if_top);

	cam_A_clk = clk_get(&pdev->dev, "sclk_cam0");
	if (IS_ERR(cam_A_clk))
		return PTR_ERR(cam_A_clk);

	clk_disable(cam_A_clk);
	clk_put(cam_A_clk);


	cam_A_clk = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(cam_A_clk))
		return PTR_ERR(cam_A_clk);

	clk_disable(cam_A_clk);
	clk_put(cam_A_clk);


	return 0;
}

int exynos5_fimc_is_regulator_on(struct platform_device *pdev)
{
	struct regulator *regulator = NULL;
	struct exynos5_platform_fimc_is *dev = pdev->dev.platform_data;

	if (dev->regulator_info) {
		/* ldo17 */
		regulator = regulator_get(NULL, dev->regulator_info->cam_core);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		regulator_enable(regulator);
		regulator_put(regulator);

		/* ldo18 */
		regulator = regulator_get(NULL, dev->regulator_info->cam_io);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		regulator_enable(regulator);
		regulator_put(regulator);


		/* ldo24 */
		regulator = regulator_get(NULL, dev->regulator_info->cam_af);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		regulator_enable(regulator);
		regulator_put(regulator);



		/* ldo19 */
		regulator = regulator_get(NULL, dev->regulator_info->cam_vt);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		regulator_enable(regulator);
		regulator_put(regulator);
	}

	return 0;
}

int exynos5_fimc_is_regulator_off(struct platform_device *pdev)
{
	struct regulator *regulator = NULL;
	struct exynos5_platform_fimc_is *dev = pdev->dev.platform_data;

	if (dev->regulator_info) {
		regulator = regulator_get(NULL, dev->regulator_info->cam_vt);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);

		regulator_put(regulator);

		regulator = regulator_get(NULL, dev->regulator_info->cam_af);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);

		regulator_put(regulator);

		regulator = regulator_get(NULL, dev->regulator_info->cam_io);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);

		regulator_put(regulator);

		regulator = regulator_get(NULL, dev->regulator_info->cam_core);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s : regulator_get fail\n", __func__);
			return PTR_ERR(regulator);
		}
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);

		regulator_put(regulator);

	}

	return 0;
}
