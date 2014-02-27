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

#include "../../../sound/soc/codecs/ymu831/ymu831_priv.h"


#define YMU831_BUS_NUM 2


extern unsigned int system_rev;

static void ymu831_set_micbias(int en)
{
	gpio_set_value(EXYNOS5410_GPJ1(2), en);
}

#ifdef CONFIG_SND_USE_YMU831_LDODE_GPIO
static void ymu831_set_ldod(int status)
{
	gpio_set_value(GPIO_YMU_LDO_EN, status);
}
#endif

static struct mc_asoc_platform_data mc_asoc_pdata = {
	.set_ext_micbias = ymu831_set_micbias,
#ifdef CONFIG_SND_USE_YMU831_LDODE_GPIO
	.set_codec_ldod = ymu831_set_ldod,
#endif
};

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


static struct platform_device ymu831_snd_card_device = {
	.name	= "ymu831-card",
	.id	= -1,
};

static struct platform_device *universal5410_audio_devices[] __initdata = {
	&s3c64xx_device_spi2,
	&ymu831_snd_card_device,
};

static void vienna_audio_gpio_init(void)
{
	int err;

	if (system_rev < 0x2) {
		/* codec spi_mode */
		err = gpio_request(EXYNOS5410_GPB2(1), "CODEC_SPIMODE");
		if (err) {
			pr_err(KERN_ERR "GPIO_CODEC_SPI_MODE GPIO set error!\n");
			return;
		}
		gpio_direction_output(EXYNOS5410_GPB2(1), 1);
		gpio_set_value(EXYNOS5410_GPB2(1), 0); /* codec spi_mode 0, 3 */
		gpio_free(EXYNOS5410_GPB2(1));
	}

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

	/* Main Microphone BIAS */
	err = gpio_request(EXYNOS5410_GPJ1(2), "MICBIAS");
	if (err) {
		pr_err(KERN_ERR "MICBIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(EXYNOS5410_GPJ1(2), 1);
	gpio_set_value(EXYNOS5410_GPJ1(2), 0);
	gpio_free(EXYNOS5410_GPJ1(2));

	err = gpio_request(EXYNOS5410_GPJ0(1), "SPK_EN");
	if (err) {
		pr_err(KERN_ERR "SPK_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(EXYNOS5410_GPJ0(1), 1);
	gpio_set_value(EXYNOS5410_GPJ0(1), 1);
	gpio_free(EXYNOS5410_GPJ0(1));

	err = gpio_request(EXYNOS5410_GPK0(7), "LINEOUT_ON");
	if (err) {
		pr_err(KERN_ERR "LINEOUT_ON GPIO set error!\n");
		return;
	}
	gpio_direction_output(EXYNOS5410_GPK0(7), 1);
	gpio_set_value(EXYNOS5410_GPK0(7), 1);
	gpio_free(EXYNOS5410_GPK0(7));
}

void __init exynos5_universal5410_audio_init(void)
{
	int ret;

	exynos5_audio_init();
	vienna_audio_gpio_init();

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

	platform_add_devices(universal5410_audio_devices,
			ARRAY_SIZE(universal5410_audio_devices));
}
