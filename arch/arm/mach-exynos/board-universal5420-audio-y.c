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

#include "board-universal5420.h"

#include "../../../sound/soc/codecs/ymu831/ymu831_priv.h"


#define YMU831_BUS_NUM 2


static void ymu831_set_micbias(int en)
{
#ifdef GPIO_MICBIAS_EN
	gpio_set_value(GPIO_MICBIAS_EN, en);
#endif
}

static void ymu831_set_ldod(int status)
{
#ifdef GPIO_YMU_LDO_EN
	gpio_set_value(GPIO_YMU_LDO_EN, status);
#endif
}

static struct mc_asoc_platform_data mc_asoc_pdata = {
	.set_ext_micbias = ymu831_set_micbias,
	.set_codec_ldod = ymu831_set_ldod,
	.irq = IRQ_EINT(21),
};

static struct s3c64xx_spi_csinfo spi_audio_csi[] = {
	[0] = {
		.line		= EXYNOS5420_GPB1(2),
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
		.controller_data	= &spi_audio_csi[0],
	}
};

static struct platform_device ymu831_snd_card_device = {
	.name	= "ymu831-card",
	.id	= -1,
};

static struct platform_device *universal5420_audio_devices[] __initdata = {
	&s3c64xx_device_spi2,
	&ymu831_snd_card_device,
};

static void universal5420_audio_gpio_init(void)
{
	int err;

#ifdef GPIO_EAR_SEND_END
	/* Interrupt from codec jack detection */
	err = gpio_request(GPIO_EAR_SEND_END, "EAR_SEND_END");
	if (err) {
		pr_err(KERN_ERR "EAR_SEND_END GPIO set error!\n");
		return;
	}
	s3c_gpio_setpull(GPIO_EAR_SEND_END, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_EAR_SEND_END);
	gpio_direction_input(GPIO_EAR_SEND_END);
	s3c_gpio_cfgpin(GPIO_EAR_SEND_END, S3C_GPIO_SFN(0xf));
#endif
#ifdef GPIO_YMU_LDO_EN
	/* Codec ldod control */
	err = gpio_request(GPIO_YMU_LDO_EN, "YMU_LDO_EN");
	if (err) {
		pr_err(KERN_ERR "YMU_LDO_EN GPIO set error!\n");
		return;
	}
	s3c_gpio_setpull(GPIO_YMU_LDO_EN, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_YMU_LDO_EN, 1);
	gpio_set_value(GPIO_YMU_LDO_EN, 0);
#endif
#ifdef GPIO_MICBIAS_EN
	/* Main microphone bias */
	err = gpio_request(GPIO_MICBIAS_EN, "MICBIAS_EN");
	if (err) {
		pr_err(KERN_ERR "MICBIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_MICBIAS_EN, 1);
	gpio_set_value(GPIO_MICBIAS_EN, 0);
#endif
}

void __init exynos5_universal5420_audio_init(void)
{
	int ret;

	exynos5_audio_init();
	universal5420_audio_gpio_init();

	ret = exynos_spi_cfg_cs(spi_audio_csi[0].line, 2);
	if (!ret) {
		s3c64xx_spi2_set_platdata(&s3c64xx_spi2_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi_audio_csi));

	} else
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH2 CS\n");

	ret = spi_register_board_info(spi_board_info,
						ARRAY_SIZE(spi_board_info));
	if (ret)
		pr_err("ERR! spi_register_board_info fail (err %d)\n", ret);

	platform_add_devices(universal5420_audio_devices,
			ARRAY_SIZE(universal5420_audio_devices));
}
