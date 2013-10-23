/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/s3c64xx-spi.h>

#include <mach/irqs.h>
#include <mach/spi-clocks.h>
#include <mach/exynos5-audio.h>

#include "board-universal5410.h"

#include "../../../sound/soc/codecs/ymu831/ymu831_priv.h"


#undef ADONISUNIV_SPI_EMUL
#define YMU831_BUS_NUM 2


static void ymu831_set_micbias(int en)
{
#ifdef GPIO_MICBIAS_EN
#ifdef CONFIG_MACH_HA
	if (system_rev < 0x1)
		gpio_set_value(GPIO_SUB_MICBIAS_EN, en);
	else
		gpio_set_value(GPIO_MICBIAS_EN, en);
#else
	gpio_set_value(GPIO_MICBIAS_EN, en);
#endif
#endif
}

static void ymu831_set_sub_micbias(int en)
{
#ifdef GPIO_SUB_MICBIAS_EN
#ifdef CONFIG_MACH_HA
	if (system_rev < 0x1)
		gpio_set_value(GPIO_MICBIAS_EN, en);
	else
		gpio_set_value(GPIO_SUB_MICBIAS_EN, en);
#else
	gpio_set_value(GPIO_SUB_MICBIAS_EN, en);
#endif
#endif
}

#ifdef CONFIG_SND_USE_YMU831_LDODE_GPIO
static void ymu831_set_ldod(int status)
{
	gpio_set_value(GPIO_YMU_LDO_EN, status);
}
#endif

static struct mc_asoc_platform_data mc_asoc_pdata = {
	.set_ext_micbias = ymu831_set_micbias,
	.set_ext_sub_micbias = ymu831_set_sub_micbias,
#ifdef CONFIG_SND_USE_YMU831_LDODE_GPIO
	.set_codec_ldod = ymu831_set_ldod,
#endif
};

#ifndef ADONISUNIV_SPI_EMUL
static struct s3c64xx_spi_csinfo spi_audio_csi[] = {
	[0] = {
		.line		= EXYNOS5410_GPB1(2),
		.set_level	= gpio_set_value,
		.fb_delay	= 0x2,
	},
};

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias		= "ymu831",
		.platform_data		= &mc_asoc_pdata,
		.max_speed_hz		= 10*1000*1000,
		.bus_num		= YMU831_BUS_NUM,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq			= IRQ_EINT(21),
		.controller_data	= &spi_audio_csi[0],
	}
};
#else
static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias		= "ymu831",
		.platform_data		= &mc_asoc_pdata,
		.max_speed_hz		= 1200000,
		.bus_num		= YMU831_BUS_NUM,
		.chip_select		= 0,
		.mode			= SPI_MODE_3,
		.irq			= IRQ_EINT(21),
		.controller_data	= (void *)EXYNOS5410_GPB1(2),
	}
};

static struct spi_gpio_platform_data spi_gpio_data = {
	.sck	= EXYNOS5410_GPB1(1),
	.mosi	= EXYNOS5410_GPB1(4),
	.miso	= EXYNOS5410_GPB1(3),
	.num_chipselect = 1,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= YMU831_BUS_NUM,
	.dev	= {
		.platform_data	= &spi_gpio_data,
	},
};
#endif

static struct platform_device ymu831_snd_card_device = {
	.name	= "ymu831-card",
	.id	= -1,
};

static struct platform_device *universal5410_audio_devices[] __initdata = {
#ifndef ADONISUNIV_SPI_EMUL
	&s3c64xx_device_spi2,
#else
	&s3c_device_spi_gpio,
#endif
	&ymu831_snd_card_device,
};

static void universal5410_audio_gpio_init(void)
{
	int err;

	/* codec spi_mode */
	err = gpio_request(EXYNOS5410_GPB2(1), "CODEC_SPIMODE");
	if (err) {
		pr_err(KERN_ERR "GPIO_CODEC_SPI_MODE GPIO set error!\n");
		return;
	}
	gpio_direction_output(EXYNOS5410_GPB2(1), 1);
	gpio_set_value(EXYNOS5410_GPB2(1), 0); /* codec spi_mode 0, 3 */
	gpio_free(EXYNOS5410_GPB2(1));

	/* Interrupt from codec jack detection */
	err = gpio_request(EXYNOS5410_GPX2(5), "EAR_SEND_END");
	if (err) {
		pr_err(KERN_ERR "EAR_SEND_END GPIO set error!\n");
		return;
	}
	s3c_gpio_setpull(EXYNOS5410_GPX2(5), S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(EXYNOS5410_GPX2(5));
	gpio_direction_input(EXYNOS5410_GPX2(5));
	s3c_gpio_cfgpin(EXYNOS5410_GPX2(5), S3C_GPIO_SFN(0xf));

#ifdef CONFIG_SND_USE_YMU831_LDODE_GPIO
	err = gpio_request(GPIO_YMU_LDO_EN, "YMU_LDO_EN");
	if (err) {
		pr_err(KERN_ERR "YMU_LDO_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_YMU_LDO_EN, 1);
	gpio_set_value(GPIO_YMU_LDO_EN, 0);
	gpio_free(GPIO_YMU_LDO_EN);
#endif

#ifdef GPIO_MICBIAS_EN
	/* Main Microphone BIAS */
	err = gpio_request(GPIO_MICBIAS_EN, "MICBIAS");
	if (err) {
		pr_err(KERN_ERR "MICBIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_MICBIAS_EN, 1);
	gpio_set_value(GPIO_MICBIAS_EN, 0);
	gpio_free(GPIO_MICBIAS_EN);
#endif

#ifdef GPIO_SUB_MICBIAS_EN
	/* Sub Microphone BIAS */
	err = gpio_request(GPIO_SUB_MICBIAS_EN, "SUB_MICBIAS");
	if (err) {
		pr_err(KERN_ERR "SUB_MICBIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_SUB_MICBIAS_EN, 1);
	gpio_set_value(GPIO_SUB_MICBIAS_EN, 0);
	gpio_free(GPIO_SUB_MICBIAS_EN);
#endif

#if defined(CONFIG_SND_DUOS_MODEM_SWITCH)
	/* DUOS PCM SEL */
	err = gpio_request(GPIO_PCM_SEL, "PCM_SEL");
	if (err) {
		pr_err(KERN_ERR "PCM_SEL GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_PCM_SEL, 1);
	gpio_set_value(GPIO_PCM_SEL, 0);
	gpio_free(GPIO_PCM_SEL);
#endif
#ifdef CONFIG_MACH_V1
	err = gpio_request(EXYNOS5410_GPJ0(1), "SPK_EN");
	if (err) {
		pr_err(KERN_ERR "SPK_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(EXYNOS5410_GPJ0(1), 1);
	gpio_set_value(EXYNOS5410_GPJ0(1), 1);
	gpio_free(EXYNOS5410_GPJ0(1));
#endif
}

void __init exynos5_universal5410_audio_init(void)
{
	int ret;

	exynos5_audio_init();
	universal5410_audio_gpio_init();

#ifndef ADONISUNIV_SPI_EMUL
	ret = exynos_spi_cfg_cs(spi_audio_csi[0].line, 2);
	if (!ret) {
		s3c64xx_spi2_set_platdata(&s3c64xx_spi2_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi_audio_csi));

	} else
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH2 CS\n");
#endif

	ret = spi_register_board_info(spi_board_info,
						ARRAY_SIZE(spi_board_info));
	if (ret)
		pr_err("ERR! spi_register_board_info fail (err %d)\n", ret);

	platform_add_devices(universal5410_audio_devices,
			ARRAY_SIZE(universal5410_audio_devices));
}
