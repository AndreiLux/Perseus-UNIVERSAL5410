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
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/arizona/pdata.h>
#include <linux/spi/spi.h>
#include <linux/input.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/s3c64xx-spi.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/spi-clocks.h>
#include <mach/exynos5-audio.h>

#include "board-universal5420.h"


#define MODE_WM5102_SPI
#undef MODE_WM5102_SPI_EMUL

#undef WM5102_SET_IMPEDANCE_DETECTION

#define WM5102_BUS_NUM 2

#include <linux/mfd/arizona/registers.h>

static struct platform_device wm5102_snd_card_device = {
	.name	= "wm5102-card",
	.id	= -1,
};

static void wm5102_init_done(void)
{
	platform_device_register(&wm5102_snd_card_device);
}

#ifdef WM5102_SET_IMPEDANCE_DETECTION
extern void adonisuniv_wm5102_hpdet_cb(unsigned int measurement);
#endif

static struct arizona_micd_config wm5102_micd[] = {
	{ 0,                  1 << ARIZONA_MICD_BIAS_SRC_SHIFT, 0 },
};

static const struct arizona_micd_range adonisuniv_micd_ranges[] = {
	{ .max =  139, .key = KEY_MEDIA },
	{ .max =  295, .key = KEY_VOLUMEUP },
	{ .max =  752, .key = KEY_VOLUMEDOWN },
	{ .max = 1257, .key = KEY_ESC },
};

static struct arizona_pdata arizona_platform_data = {
	.reset = GPIO_CODEC_RESET,
	.ldoena = GPIO_CODEC_LDO_EN,
	.irq_base = IRQ_BOARD_AUDIO_START,
	.irq_flags = IRQF_TRIGGER_HIGH,
	.micd_configs = wm5102_micd,
	.num_micd_configs = ARRAY_SIZE(wm5102_micd),
	.micd_force_micbias = true,
/*
	.micd_level = {0x3f3f, 0x3f3f, 0x3b3f, 0x2832},
*/
	.micd_ranges = adonisuniv_micd_ranges,
	.num_micd_ranges = ARRAY_SIZE(adonisuniv_micd_ranges),
	.micd_bias_start_time = 0x7,
	.micd_rate = 0x7,
	.micd_dbtime = 0x1,
	.micd_timeout = 300,
	.micbias = {
		[0] = { 2600, 0, 1, 0, 0},
		[1] = { 2600, 0, 1, 0, 0},
		[2] = { 2600, 0, 1, 0, 0},
	},
	.gpio_base = S3C_GPIO_END + 1,
#ifdef WM5102_SET_IMPEDANCE_DETECTION
	.hpdet_cb = adonisuniv_wm5102_hpdet_cb,
#endif
	.hpdet_id_gpio = S3C_GPIO_END + 3,
	.hpdet_acc_id = true,
	.jd_gpio5 = true,
	.jd_gpio5_nopull = true,
	.init_done = wm5102_init_done,
	.max_channels_clocked = {
	[0] = 2,
	}
};

#ifdef MODE_WM5102_SPI
static struct s3c64xx_spi_csinfo spi_audio_csi[] = {
	[0] = {
		.line		= GPIO_CODEC_SPI_SS_N,
		.set_level	= gpio_set_value,
		.fb_delay	= 0x2,
	},
};

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias		= "wm5102",
		.platform_data		= &arizona_platform_data,
		.max_speed_hz		= 10*1000*1000,
		.bus_num		= WM5102_BUS_NUM,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq			= IRQ_EINT(21),
		.controller_data	= &spi_audio_csi[0],
	}
};
#elif defined(MODE_WM5102_SPI_EMUL)
static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias		= "arizona",
		.platform_data		= &arizona_platform_data
		.max_speed_hz		= 1200000,
		.bus_num		= WM5102_BUS_NUM,
		.chip_select		= 0,
		.mode			= SPI_MODE_3,
		.irq			= IRQ_EINT(21),
		.controller_data	= (void *)GPIO_CODEC_SPI_SS_N,
	}
};

static struct spi_gpio_platform_data spi_gpio_data = {
	.sck	= GPIO_CODEC_SPI_SCK,
	.mosi	= GPIO_CODEC_SPI_MOSI,
	.miso	= GPIO_CODEC_SPI_MISO,
	.num_chipselect = 1,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= WM5102_BUS_NUM,
	.dev	= {
		.platform_data	= &spi_gpio_data,
	},
};
#endif

static struct platform_device *universal5420_audio_devices[] __initdata = {
#ifndef MODE_WM5102_SPI_EMUL
	&s3c64xx_device_spi2,
#else
	&s3c_device_spi_gpio,
#endif
};

static void universal5420_audio_gpio_init(void)
{

        int err;
	err = gpio_request(GPIO_CODEC_RESET, "CODEC RESET enable");
	if (err < 0)
		pr_err("%s: Failed to get enable GPIO: %d\n", __func__, err);

	err = gpio_direction_output(GPIO_CODEC_RESET, 1);
	if (err < 0)
		pr_err("%s: Failed to set GPIO direction: %d\n", __func__, err);

	gpio_set_value(GPIO_CODEC_RESET, 0);
	gpio_free(GPIO_CODEC_RESET);

}

void __init exynos5_universal5420_audio_init(void)
{

	int ret;
	exynos5_audio_init();
	universal5420_audio_gpio_init();

#ifdef MODE_WM5102_SPI
	ret = exynos_spi_cfg_cs(spi_audio_csi[0].line, 2);
	if (!ret) {
		s3c64xx_spi2_set_platdata(&s3c64xx_spi2_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi_audio_csi));
	} else
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH2 CS\n");
#endif

#if defined(MODE_WM5102_SPI) || defined(MODE_WM5102_SPI_EMUL)
	ret = spi_register_board_info(spi_board_info,
						ARRAY_SIZE(spi_board_info));
	if (ret)
		pr_err("ERR! spi_register_board_info fail (err %d)\n", ret);
#endif

	platform_add_devices(universal5420_audio_devices,
			ARRAY_SIZE(universal5420_audio_devices));

}
