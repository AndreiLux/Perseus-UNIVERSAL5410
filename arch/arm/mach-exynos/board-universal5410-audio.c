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
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/s3c64xx-spi.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/spi-clocks.h>

#include "board-universal5410.h"
#if defined(CONFIG_SND_SOC_ADONISUNIV_YMU831) || defined(CONFIG_SND_SOC_ADONISUNIV_YMU831_SLSI)
#include "../../../sound/soc/codecs/ymu831/ymu831_priv.h"
#endif
#undef ADONISUNIV_SPI_EMUL

#ifdef CONFIG_SND_SOC_ADONISUNIV_WM1811
static struct regulator_consumer_supply wm8994_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("AVDD2", "4-001a"),
	REGULATOR_SUPPLY("CPVDD", "4-001a"),
};

static struct regulator_consumer_supply wm8994_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDD1", "4-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "4-001a"),
};

static struct regulator_consumer_supply wm8994_fixed_voltage2_supplies =
	REGULATOR_SUPPLY("DBVDD", "4-001a");

static struct regulator_init_data wm8994_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage0_supplies),
	.consumer_supplies	= wm8994_fixed_voltage0_supplies,
};

static struct regulator_init_data wm8994_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage1_supplies),
	.consumer_supplies	= wm8994_fixed_voltage1_supplies,
};

static struct regulator_init_data wm8994_fixed_voltage2_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_fixed_voltage2_supplies,
};

static struct fixed_voltage_config wm8994_fixed_voltage0_config = {
	.supply_name	= "VDD_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm8994_fixed_voltage1_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage1_init_data,
};

static struct fixed_voltage_config wm8994_fixed_voltage2_config = {
	.supply_name	= "VDD_3.3V",
	.microvolts	= 3300000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage2_init_data,
};

static struct platform_device wm8994_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage0_config,
	},
};

static struct platform_device wm8994_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage1_config,
	},
};

static struct platform_device wm8994_fixed_voltage2 = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage2_config,
	},
};

static struct regulator_consumer_supply wm8994_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "4-001a");

static struct regulator_consumer_supply wm8994_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "4-001a");

static struct regulator_init_data wm8994_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_avdd1_supply,
};

static struct regulator_init_data wm8994_ldo2_data = {
	.constraints	= {
		.name		= "DCVDD",
		.always_on	= true,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_dcvdd_supply,
};

static struct wm8994_pdata wm8994_platform_data = {
	/* configure gpio1 function: 0x0001(Logic level input/output) */
	.gpio_defaults[0] = 0x0001,
	/* If the i2s0 and i2s2 is enabled simultaneously */
	.gpio_defaults[7] = 0x8100, /* GPIO8  DACDAT3 in */
	.gpio_defaults[8] = 0x0100, /* GPIO9  ADCDAT3 out */
	.gpio_defaults[9] = 0x0100, /* GPIO10 LRCLK3  out */
	.gpio_defaults[10] = 0x0100,/* GPIO11 BCLK3   out */
	.ldo[0] = { 0, &wm8994_ldo1_data }, /* LDO1,LDO2 Enable */
	.ldo[1] = { 0, &wm8994_ldo2_data },
	.irq_base = IRQ_BOARD_AUDIO_START
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x1a),
		.platform_data	= &wm8994_platform_data,
		.irq = IRQ_EINT(21),
	},
};

#endif

#if defined(CONFIG_SND_SOC_ADONISUNIV_YMU831) || defined(CONFIG_SND_SOC_ADONISUNIV_YMU831_SLSI)
static void ymu831_set_micbias(int en)
{
	gpio_set_value(EXYNOS5410_GPJ1(2), en);
}


static struct mc_asoc_platform_data mc_asoc_pdata = {
	.set_ext_micbias = ymu831_set_micbias,
};
#ifndef ADONISUNIV_SPI_EMUL
static struct s3c64xx_spi_csinfo spi2_csi[] = {
	[0] = {
		.line		= EXYNOS5410_GPB1(2),
		.set_level	= gpio_set_value,
		.fb_delay	= 0x2,
	},
};


static struct spi_board_info spi2_board_info[] __initdata = {
	{
		.modalias		= "ymu831",
		.platform_data		= &mc_asoc_pdata,
		.max_speed_hz		= 15*1000*1000,
		.bus_num		= 2,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq			= IRQ_EINT(21),
		.controller_data	= &spi2_csi[0],
	}
};
#else

#define YMU831_BUS_NUM 2

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

static struct spi_gpio_platform_data ymu831_spi_gpio_data = {
	.sck	= EXYNOS5410_GPB1(1),
	.mosi	= EXYNOS5410_GPB1(4),
	.miso	= EXYNOS5410_GPB1(3),
	.num_chipselect = 1,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= YMU831_BUS_NUM,
	.dev	= {
		.platform_data	= &ymu831_spi_gpio_data,
	},
};
#endif
#endif

static struct platform_device *universal5410_audio_devices[] __initdata = {
#ifdef CONFIG_SND_SOC_ADONISUNIV_WM1811
	&exynos5_device_hs_i2c0,

	&wm8994_fixed_voltage0,
	&wm8994_fixed_voltage1,
	&wm8994_fixed_voltage2,
#endif
#ifdef CONFIG_SND_SAMSUNG_AC97
	&exynos_device_ac97,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos5_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos5_device_pcm0,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos5_device_spdif,
#endif
#if defined(CONFIG_SND_SAMSUNG_RP) || defined(CONFIG_SND_SAMSUNG_ALP)
	&exynos5_device_srp,
#endif
#if defined(CONFIG_SND_SOC_ADONISUNIV_YMU831) || defined(CONFIG_SND_SOC_ADONISUNIV_YMU831_SLSI)
#ifdef CONFIG_S3C64XX_DEV_SPI2
#ifndef ADONISUNIV_SPI_EMUL
	&s3c64xx_device_spi2,
#else
	&s3c_device_spi_gpio,
#endif
#endif
#endif
	&samsung_asoc_dma,
	&samsung_asoc_idma,
};

static void universal5410_audio_setup_clocks(void)
{
	struct clk *xxti = NULL;
	struct clk *clkout = NULL;

	xxti = clk_get(NULL, "xxti");
	if (!xxti) {
		pr_err("%s: cannot get xxti clock\n", __func__);
		return;
	}

	clkout = clk_get(NULL, "clkout");
	if (!clkout) {
		pr_err("%s: cannot get clkout\n", __func__);
		clk_put(xxti);
		return;
	}

	clk_set_parent(clkout, xxti);
	clk_put(clkout);
	clk_put(xxti);
}

struct exynos5_platform_i2c hs_i2c0_data __initdata = {
	.bus_number = 4,
	.operation_mode = 0,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 0,
	.cfg_gpio = NULL,
};

static void universal5410_audio_gpio_init(void)
{
	int err;

#ifdef CONFIG_SND_SOC_ADONISUNIV_WM1811
	err = gpio_request(EXYNOS5410_GPJ3(7), "CODEC LDO enable");
	if (err < 0)
		pr_err("%s: Failed to get enable GPIO: %d\n", __func__, err);

	err = gpio_direction_output(EXYNOS5410_GPJ3(7), 1);
	if (err < 0)
		pr_err("%s: Failed to set GPIO up: %d\n", __func__, err);

	gpio_set_value(EXYNOS5410_GPJ3(7), 1);

	exynos5_hs_i2c0_set_platdata(&hs_i2c0_data);
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
#endif

#if defined(CONFIG_SND_SOC_ADONISUNIV_YMU831) || defined(CONFIG_SND_SOC_ADONISUNIV_YMU831_SLSI)
	/* Main Microphone BIAS */
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

	/* codec spi_mode */
	err = gpio_request(EXYNOS5410_GPJ1(2),  "MICBIAS");
	if (err) {
		pr_err(KERN_ERR "MICBIAS_EN GPIO set error!\n");
		return;
	}

	gpio_direction_output(EXYNOS5410_GPJ1(2), 1);
	gpio_set_value(EXYNOS5410_GPJ1(2), 0);
	gpio_free(EXYNOS5410_GPJ1(2));

#endif
}

void __init exynos5_universal5410_audio_init(void)
{
	int ret;

	universal5410_audio_setup_clocks();

	universal5410_audio_gpio_init();


#ifdef CONFIG_SND_SOC_ADONISUNIV_WM1811
	exynos5_hs_i2c0_set_platdata(&hs_i2c0_data);
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
#endif

#if defined(CONFIG_SND_SOC_ADONISUNIV_YMU831) || defined(CONFIG_SND_SOC_ADONISUNIV_YMU831_SLSI)
#ifndef ADONISUNIV_SPI_EMUL
	ret = exynos_spi_cfg_cs(spi2_csi[0].line, 2);
	if (!ret) {
		s3c64xx_spi2_set_platdata(&s3c64xx_spi2_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi2_csi));

		spi_register_board_info(spi2_board_info,
				ARRAY_SIZE(spi2_board_info));
	} else
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH2 CS\n");
#else
	ret = spi_register_board_info(spi_board_info,
			ARRAY_SIZE(spi_board_info));

	if (ret)
		pr_err("ERR! spi_register_board_info fail (err %d)\n",  ret);
#endif
#endif

	platform_add_devices(universal5410_audio_devices,
			ARRAY_SIZE(universal5410_audio_devices));
}
