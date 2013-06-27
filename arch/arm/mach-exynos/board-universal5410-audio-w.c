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

#include "board-universal5410.h"


#define MODE_WM5102_SPI
#undef MODE_WM5102_SPI_EMUL
#undef MODE_WM5102_I2C_EMUL

#undef WM5102_SET_IMPEDANCE_DETECTION

#define WM5102_BUS_NUM 2


#ifdef CONFIG_SND_SOC_WM8994
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

static struct i2c_board_info i2c_devs_audio[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x1a),
		.platform_data	= &wm8994_platform_data,
		.irq = IRQ_EINT(21),
	},
};

static struct platform_device wm1811_snd_card_device = {
	.name	= "wm1811-card",
	.id	= -1,
};

static struct platform_device *universal5410_audio_devices[] __initdata = {
	&exynos5_device_hs_i2c0,

	&wm8994_fixed_voltage0,
	&wm8994_fixed_voltage1,
	&wm8994_fixed_voltage2,

	&wm1811_snd_card_device,
};

struct exynos5_platform_i2c hs_i2c_data_audio __initdata = {
	.bus_number = 4,
	.operation_mode = 0,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 0,
	.cfg_gpio = NULL,
};
#endif

#ifdef CONFIG_SND_SOC_WM5102
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
	.reset = EXYNOS5410_GPJ2(1),
	.ldoena = EXYNOS5410_GPJ1(5),
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
		.line		= EXYNOS5410_GPB1(2),
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
#elif defined(MODE_WM5102_I2C_EMUL)
#include <linux/i2c-gpio.h>
static struct i2c_gpio_platform_data gpio_i2c_data12 = {
	.scl_pin = EXYNOS5410_GPK2(2),
	.sda_pin = EXYNOS5410_GPK2(3),
};

struct platform_device s3c_device_i2c12 = {
	.name = "i2c-gpio",
	.id = 12,
	.dev.platform_data = &gpio_i2c_data12,
};

static struct i2c_board_info i2c_devs12_emul[] __initdata = {
	{
		I2C_BOARD_INFO("wm5102", 0x1a),
		.platform_data	= &arizona_platform_data,
		.irq		= IRQ_EINT(21),
	},
};
#endif

static struct platform_device *universal5410_audio_devices[] __initdata = {
#ifndef MODE_WM5102_SPI_EMUL
	&s3c64xx_device_spi2,
#else
	&s3c_device_spi_gpio,
#endif
};
#endif

static void universal5410_audio_gpio_init(void)
{
        int err;

#ifdef CONFIG_SND_SOC_WM1811
	err = gpio_request(EXYNOS5410_GPJ3(7), "CODEC LDO enable");
	if (err < 0)
		pr_err("%s: Failed to get enable GPIO: %d\n", __func__, err);

	err = gpio_direction_output(EXYNOS5410_GPJ3(7), 1);
	if (err < 0)
		pr_err("%s: Failed to set GPIO direction: %d\n", __func__, err);

	gpio_set_value(EXYNOS5410_GPJ3(7), 1);
#endif

#ifdef CONFIG_SND_SOC_WM5102
	err = gpio_request(EXYNOS5410_GPJ2(1), "CODEC RESET enable");
	if (err < 0)
		pr_err("%s: Failed to get enable GPIO: %d\n", __func__, err);

	err = gpio_direction_output(EXYNOS5410_GPJ2(1), 1);
	if (err < 0)
		pr_err("%s: Failed to set GPIO direction: %d\n", __func__, err);

	gpio_set_value(EXYNOS5410_GPJ2(1), 0);
	gpio_free(EXYNOS5410_GPJ2(1));
#endif
}

void __init exynos5_universal5410_audio_init(void)
{
	int ret;

	exynos5_audio_init();
	universal5410_audio_gpio_init();

#ifdef CONFIG_SND_SOC_WM1811
	exynos5_hs_i2c0_set_platdata(&hs_i2c_data_audio);
	ret = i2c_register_board_info(4, i2c_devs_audio, ARRAY_SIZE(i2c_devs_audio));
#endif

#ifdef CONFIG_SND_SOC_WM5102
	s3c_gpio_cfgpin(EXYNOS5410_GPJ1(4), S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(EXYNOS5410_GPJ1(4), S3C_GPIO_PULL_NONE);

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

#ifdef MODE_WM5102_I2C_EMUL
	//i2c_devs12_emul[0].irq = gpio_to_irq(EXYNOS5410_GPJ1(4));
	platform_device_register(&s3c_device_i2c12);
	ret = i2c_register_board_info(12, i2c_devs12_emul,
				ARRAY_SIZE(i2c_devs12_emul));
#endif
#endif

	platform_add_devices(universal5410_audio_devices,
			ARRAY_SIZE(universal5410_audio_devices));

	exynos5_universal5410_2mic_init();
}
