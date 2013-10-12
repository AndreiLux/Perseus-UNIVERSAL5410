/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <video/platform_lcd.h>
#include <video/s5p-dp.h>

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/dp.h>
#include <plat/backlight.h>
#include <plat/gpio-cfg.h>

#include <mach/map.h>

#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#endif

unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);
static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);

#ifdef CONFIG_UNIVERSAL5410_REAL_REV00
	lcdtype = 0x2000;
#else
	lcdtype = 0x3000;
#endif
	return 1;
}
__setup("lcdtype=", lcdtype_setup);

#if defined(CONFIG_LCD_MIPI_AMS480GYXX)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* reset */
	if (power) {
		gpio_request_one(GPIO_MLCD_RST,
				GPIOF_OUT_INIT_HIGH, "GPJ1");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5410_GPJ1(0), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5410_GPJ1(0), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));
	} else {
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPJ1");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));
	}

	/* enable VCC_2.2V_LCD */
	if (power) {
		gpio_request_one(EXYNOS5410_GPJ2(0),
				GPIOF_OUT_INIT_HIGH, "GPJ2");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ2(0));
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPF1");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
	} else {
		gpio_request_one(EXYNOS5410_GPJ2(0),
				GPIOF_OUT_INIT_LOW, "GPJ2");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ2(0));
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPF1");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
	}
}

static struct plat_lcd_data universal5410_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device universal5410_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &universal5410_mipi_lcd_data,
};

static struct s3c_fb_pd_win universal5410_fb_win0 = {
	.win_mode = {
		.left_margin	= 0x9,
		.right_margin	= 0x9,
		.upper_margin	= 0x3,
		.lower_margin	= 0xA,
		.hsync_len	= 0x2,
		.vsync_len	= 0x2,
		.xres		= 720,
		.yres		= 1280,
	},
	.virtual_x		= 720,
	.virtual_y		= 1280 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win1 = {
	.win_mode = {
		.left_margin	= 0x9,
		.right_margin	= 0x9,
		.upper_margin	= 0x3,
		.lower_margin	= 0xA,
		.hsync_len	= 0x2,
		.vsync_len	= 0x2,
		.xres		= 720,
		.yres		= 1280,
	},
	.virtual_x		= 720,
	.virtual_y		= 1280 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win2 = {
	.win_mode = {
		.left_margin	= 0x9,
		.right_margin	= 0x9,
		.upper_margin	= 0x3,
		.lower_margin	= 0xA,
		.hsync_len	= 0x2,
		.vsync_len	= 0x2,
		.xres		= 720,
		.yres		= 1280,
	},
	.virtual_x		= 720,
	.virtual_y		= 1280 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win3 = {
	.win_mode = {
		.left_margin	= 0x9,
		.right_margin	= 0x9,
		.upper_margin	= 0x3,
		.lower_margin	= 0xA,
		.hsync_len	= 0x2,
		.vsync_len	= 0x2,
		.xres		= 720,
		.yres		= 1280,
	},
	.virtual_x		= 720,
	.virtual_y		= 1280 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win4 = {
	.win_mode = {
		.left_margin	= 0x9,
		.right_margin	= 0x9,
		.upper_margin	= 0x3,
		.lower_margin	= 0xA,
		.hsync_len	= 0x2,
		.vsync_len	= 0x2,
		.xres		= 720,
		.yres		= 1280,
	},
	.virtual_x		= 720,
	.virtual_y		= 1280 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#define mipi_lcd_power_controlNULL
#elif defined(CONFIG_LCD_MIPI_ER63311)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, "touch_1.8v_s");

	if (IS_ERR(regulator))
		printk(KERN_ERR "LCD: Fail to get regulator!\n");

	if (power) {
		/*mipi 1.8v enable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));

		/*lcd 1.8V enable*/
		regulator_enable(regulator);
		usleep_range(2000, 3000);

		/*AVDD enable*/
		gpio_request_one(EXYNOS5410_GPJ2(0),
				GPIOF_OUT_INIT_HIGH, "GPIO_LCD_22V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ2(0));
		usleep_range(2000, 3000);

		/*LCD RESET high*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
		GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));
		usleep_range(7000, 8000);
	} else {
		/*LCD RESET low*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));

		/*AVDD disable*/
		gpio_request_one(EXYNOS5410_GPJ2(0),
				GPIOF_OUT_INIT_LOW, "GPIO_LCD_22V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ2(0));

		/*lcd 1.8V disable*/
		if (regulator_is_enabled(regulator))
				regulator_disable(regulator);

		/*mipi 1.8v disable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
	}
	regulator_put(regulator);
}

static void mipi_lcd_power_control(struct s5p_platform_mipi_dsim *dsim,
				unsigned int power)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, "touch_1.8v_s");

	if (IS_ERR(regulator))
		printk(KERN_ERR "LCD: Fail to get regulator!\n");

	if (power) {
		/*mipi 1.8v enable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));

		/*lcd 1.8V enable*/
		regulator_enable(regulator);
		usleep_range(2000, 3000);

		/*AVDD enable*/
		gpio_request_one(EXYNOS5410_GPJ2(0),
				GPIOF_OUT_INIT_HIGH, "GPIO_LCD_22V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ2(0));
		usleep_range(2000, 3000);

		/*LCD RESET high*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
		GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));
		usleep_range(7000, 8000);
	} else {
		/*LCD RESET low*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));

		/*AVDD disable*/
		gpio_request_one(EXYNOS5410_GPJ2(0),
				GPIOF_OUT_INIT_LOW, "GPIO_LCD_22V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ2(0));

		/*lcd 1.8V disable*/
		if (regulator_is_enabled(regulator))
				regulator_disable(regulator);

		/*mipi 1.8v disable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
	}
	regulator_put(regulator);
}

static struct plat_lcd_data universal5410_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device universal5410_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &universal5410_mipi_lcd_data,
};

static struct s3c_fb_pd_win universal5410_fb_win0 = {
	.win_mode = {
		.left_margin	= 100,
		.right_margin	= 250,
		.upper_margin	= 4,
		.lower_margin	= 42,
		.hsync_len	= 20,
		.vsync_len	= 14,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1088,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win1 = {
	.win_mode = {
		.left_margin	= 100,
		.right_margin	= 250,
		.upper_margin	= 4,
		.lower_margin	= 42,
		.hsync_len	= 20,
		.vsync_len	= 14,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1088,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win2 = {
	.win_mode = {
		.left_margin	= 100,
		.right_margin	= 250,
		.upper_margin	= 4,
		.lower_margin	= 42,
		.hsync_len	= 20,
		.vsync_len	= 14,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1080,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
static struct s3c_fb_pd_win universal5410_fb_win3 = {
	.win_mode = {
		.left_margin	= 100,
		.right_margin	= 250,
		.upper_margin	= 4,
		.lower_margin	= 42,
		.hsync_len	= 20,
		.vsync_len	= 14,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1080,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win4 = {
	.win_mode = {
		.left_margin	= 100,
		.right_margin	= 250,
		.upper_margin	= 4,
		.lower_margin	= 42,
		.hsync_len	= 20,
		.vsync_len	= 14,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1080,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#elif defined(CONFIG_LCD_MIPI_S6E8FA0)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	struct regulator *regulator_1_8;
	struct regulator *regulator_2_8;

	regulator_1_8 = regulator_get(NULL, "vcc_1.8v_lcd");
	regulator_2_8 = regulator_get(NULL, "vcc_2.8v_lcd");

	if (IS_ERR(regulator_1_8))
		printk(KERN_ERR "LCD_1_8: Fail to get regulator!\n");
	if (IS_ERR(regulator_2_8))
		printk(KERN_ERR "LCD_2.8: Fail to get regulator!\n");

	if (power) {
		/*mipi 1.8v enable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));

		/*panel power enable*/
		regulator_enable(regulator_2_8);

		usleep_range(5000, 8000);
		regulator_enable(regulator_1_8);
		msleep(25);

		/*LCD RESET high*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
		GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5410_GPJ1(0), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5410_GPJ1(0), 1);
		gpio_free(EXYNOS5410_GPJ1(0));
		usleep_range(7000, 8000);
	} else {
		/*LCD RESET low*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));

		/*lcd 1.8V disable*/
		if (regulator_is_enabled(regulator_1_8))
				regulator_disable(regulator_1_8);
		if (regulator_is_enabled(regulator_2_8))
				regulator_disable(regulator_2_8);

		/*mipi 1.8v disable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
	}
	regulator_put(regulator_1_8);
	regulator_put(regulator_2_8);

}

static void mipi_lcd_power_control(struct s5p_platform_mipi_dsim *dsim,
				unsigned int power)
{
	struct regulator *regulator_1_8;
	struct regulator *regulator_2_8;

	regulator_1_8 = regulator_get(NULL, "vcc_1.8v_lcd");
	regulator_2_8 = regulator_get(NULL, "vcc_2.8v_lcd");

	if (IS_ERR(regulator_1_8))
		printk(KERN_ERR "LCD_1_8: Fail to get regulator!\n");
	if (IS_ERR(regulator_2_8))
		printk(KERN_ERR "LCD_2.8: Fail to get regulator!\n");

	if (power) {
		/*mipi 1.8v enable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));

		/*panel power enable*/
		regulator_enable(regulator_2_8);

		usleep_range(5000, 8000);
		regulator_enable(regulator_1_8);
		msleep(25);

		/*LCD RESET high*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
		GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5410_GPJ1(0), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5410_GPJ1(0), 1);
		gpio_free(EXYNOS5410_GPJ1(0));
		usleep_range(7000, 8000);
	} else {
		/*LCD RESET low*/
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPJ1(0));

		/*lcd 1.8V disable*/
		if (regulator_is_enabled(regulator_1_8))
				regulator_disable(regulator_1_8);
		if (regulator_is_enabled(regulator_2_8))
				regulator_disable(regulator_2_8);

		/*mipi 1.8v disable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
	}
	regulator_put(regulator_1_8);
	regulator_put(regulator_2_8);
}

static struct plat_lcd_data universal5410_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device universal5410_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &universal5410_mipi_lcd_data,
};

/*
HBP ->left_margin
HFP ->right_margin
HSP ->hsync_len

VBP ->upper_margin
VFP ->lower_margin
VSW ->vsync_len
*/

#define HBP 16
#define HFP 40
#define HFP_DSIM 40*109.5/133
#define HSP 10
#define VBP 1
#define VFP 13
#define VSW 2

static struct s3c_fb_pd_win universal5410_fb_win0 = {
	.win_mode = {
		.left_margin	= HBP,
		.right_margin	= HFP,
		.upper_margin	= VBP,
		.lower_margin	= VFP,
		.hsync_len	= HSP,
		.vsync_len	= VSW,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1088,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win1 = {
	.win_mode = {
		.left_margin	= HBP,
		.right_margin	= HFP,
		.upper_margin	= VBP,
		.lower_margin	= VFP,
		.hsync_len	= HSP,
		.vsync_len	= VSW,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1088,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win2 = {
	.win_mode = {
		.left_margin	= HBP,
		.right_margin	= HFP,
		.upper_margin	= VBP,
		.lower_margin	= VFP,
		.hsync_len	= HSP,
		.vsync_len	= VSW,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1080,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win3 = {
	.win_mode = {
		.left_margin	= HBP,
		.right_margin	= HFP,
		.upper_margin	= VBP,
		.lower_margin	= VFP,
		.hsync_len	= HSP,
		.vsync_len	= VSW,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1080,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win4 = {
	.win_mode = {
		.left_margin	= HBP,
		.right_margin	= HFP,
		.upper_margin	= VBP,
		.lower_margin	= VFP,
		.hsync_len	= HSP,
		.vsync_len	= VSW,
		.xres		= 1080,
		.yres		= 1920,
	},
	.virtual_x		= 1080,
	.virtual_y		= 1920 * 2,
	.width			= 59,
	.height			= 104,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#elif defined(CONFIG_S5P_DP)
static void dp_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* PMIC_LDO_19_EN */
	gpio_request(EXYNOS5410_GPJ3(6), "GPJ3");
	if (power)
		gpio_direction_output(EXYNOS5410_GPJ3(6), 1);

#ifndef CONFIG_BACKLIGHT_PWM
	/* LCD_PWM_IN_2.8V: LCD_B_PWM, GPB2_0 */
	gpio_request(EXYNOS5410_GPB2(0), "GPB2");
#endif

	/* LCD_EN: GPH0_1 */
	gpio_request(EXYNOS5410_GPH0(1), "GPH0");

	/* LCD_EN: GPH0_1 */
	gpio_direction_output(EXYNOS5410_GPH0(1), power);
	msleep(90);

#ifndef CONFIG_BACKLIGHT_PWM
	/* LCD_PWM_IN_2.8V: LCD_B_PWM, GPB2_0 */
	gpio_direction_output(EXYNOS5410_GPB2(0), power);

	gpio_free(EXYNOS5410_GPB2(0));
#endif

	/* PMIC_LDO_19_EN */
	if (!power)
		gpio_direction_output(EXYNOS5410_GPJ3(6), 0);

	gpio_free(EXYNOS5410_GPH0(1));
	gpio_free(EXYNOS5410_GPJ3(6));
}

static struct plat_lcd_data universal5410_dp_lcd_data = {
	.set_power	= dp_lcd_set_power,
};

static struct platform_device universal5410_dp_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &universal5410_dp_lcd_data,
	},
};

static struct s3c_fb_pd_win universal5410_fb_win0 = {
	.win_mode = {
		.left_margin	= 80,
		.right_margin	= 48,
		.upper_margin	= 37,
		.lower_margin	= 3,
		.hsync_len	= 32,
		.vsync_len	= 6,
		.xres		= 2560,
		.yres		= 1600,
	},
	.virtual_x		= 2560,
	.virtual_y		= 1640 * 2,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win1 = {
	.win_mode = {
		.left_margin	= 80,
		.right_margin	= 48,
		.upper_margin	= 37,
		.lower_margin	= 3,
		.hsync_len	= 32,
		.vsync_len	= 6,
		.xres		= 2560,
		.yres		= 1600,
	},
	.virtual_x		= 2560,
	.virtual_y		= 1640 * 2,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win universal5410_fb_win2 = {
	.win_mode = {
		.left_margin	= 80,
		.right_margin	= 48,
		.upper_margin	= 37,
		.lower_margin	= 3,
		.hsync_len	= 32,
		.vsync_len	= 6,
		.xres		= 2560,
		.yres		= 1600,
	},
	.virtual_x		= 2560,
	.virtual_y		= 1600 * 2,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

#if defined(CONFIG_S5P_DP)
	/* Set Hotplug detect for DP */
	gpio_request(EXYNOS5410_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5410_GPX0(7), S3C_GPIO_SFN(3));
#endif

	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);

#if defined(CONFIG_S5P_DP)
	/* Reference clcok selection for DPTX_PHY: PAD_OSC_IN */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg &= ~(1 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* DPTX_PHY: XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg &= ~(1 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);
#endif
}

static struct s3c_fb_platdata universal5410_lcd1_pdata __initdata = {
#if defined(CONFIG_S5P_DP) || defined(CONFIG_LCD_MIPI_AMS480GYXX) || \
	defined(CONFIG_LCD_MIPI_ER63311) || defined(CONFIG_LCD_MIPI_S6E8FA0)
	.win[0]		= &universal5410_fb_win0,
	.win[1]		= &universal5410_fb_win1,
	.win[2]		= &universal5410_fb_win2,
	.win[3]		= &universal5410_fb_win3,
	.win[4]		= &universal5410_fb_win4,
#endif
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_LCD_MIPI_AMS480GYXX) || defined(CONFIG_LCD_MIPI_ER63311) || \
	defined(CONFIG_LCD_MIPI_S6E8FA0)
	.vidcon1	= VIDCON1_INV_VCLK,
#elif defined(CONFIG_S5P_DP)
	.vidcon1	= 0,
#endif
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.ip_version	= EXYNOS5_813,
};

#ifdef CONFIG_FB_MIPI_DSIM
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= true,
	.hse = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,
#if defined(CONFIG_LCD_MIPI_AMS480GYXX)
	.auto_vertical_cnt = true,
	.hfp = false,
	.hbp = false,

	.p = 4,
	.m = 80,
	.s = 2,
#elif defined(CONFIG_LCD_MIPI_ER63311)
	.auto_vertical_cnt = true,
	.hfp = false,
	.hbp = false,

	.p = 3,
	.m = 63,
	.s = 0,
#elif defined(CONFIG_LCD_MIPI_S6E8FA0)
	.auto_vertical_cnt = false,
	.hfp = true,
	.hbp = false,

	.p = 4,
	.m = 73,
	.s = 0,
#endif


	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 8 * MHZ,

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x01,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */
#if defined(CONFIG_LCD_MIPI_AMS480GYXX)
	.dsim_ddi_pd = &ams480gyxx_mipi_lcd_driver,
#elif defined(CONFIG_LCD_MIPI_ER63311)
	.dsim_ddi_pd = &er63311_mipi_lcd_driver,
#elif defined(CONFIG_LCD_MIPI_S6E8FA0)
	.dsim_ddi_pd = &s6e8fa0_6P_mipi_lcd_driver,
#endif
};

/* stable_vfp + cmd_allow <= lower_margin(vfp) */
#if defined(CONFIG_LCD_MIPI_AMS480GYXX)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= 0x9,
	.rgb_timing.right_margin	= 0x9,
	.rgb_timing.upper_margin	= 0x3,
	.rgb_timing.lower_margin	= 0xA,
	.rgb_timing.hsync_len		= 0x2,
	.rgb_timing.vsync_len		= 0x2,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 0xF,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= 720,
	.lcd_size.height		= 1280,
};
#elif defined(CONFIG_LCD_MIPI_ER63311)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= 100,
	.rgb_timing.right_margin	= 250,
	.rgb_timing.upper_margin	= 4,
	.rgb_timing.lower_margin	= 42,
	.rgb_timing.hsync_len		= 20,
	.rgb_timing.vsync_len		= 14,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 4,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= 1080,
	.lcd_size.height		= 1920,
};
#elif defined(CONFIG_LCD_MIPI_S6E8FA0)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= HBP,
	.rgb_timing.right_margin	= HFP_DSIM,
	.rgb_timing.upper_margin	= VBP,
	.rgb_timing.hsync_len		= HSP,
	.rgb_timing.vsync_len		= VSW,
	.rgb_timing.stable_vfp		= 3,
	.rgb_timing.cmd_allow		= 7,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= 1080,
	.lcd_size.height		= 1920,
};
#endif

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_lcd_power_control,
	.part_reset		= s5p_dsim_part_reset,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,

	/*
	 * The stable time of needing to write data on SFR
	 * when the mipi mode becomes LP mode.
	 */
	.delay_for_stabilization = 600,
};
#endif

#ifdef CONFIG_S5P_DP
static struct video_info universal5410_dp_config = {
	.name			= "WQXGA(2560x1600) LCD, for SMDK TEST",

	.h_sync_polarity	= 0,
	.v_sync_polarity	= 0,
	.interlaced		= 0,

	.color_space		= COLOR_RGB,
	.dynamic_range		= VESA,
	.ycbcr_coeff		= COLOR_YCBCR601,
	.color_depth		= COLOR_8,

	.link_rate		= LINK_RATE_2_70GBPS,
	.lane_count		= LANE_COUNT4,
};

static void s5p_dp_backlight_on(void)
{
	/* LED_BACKLIGHT_RESET: GPX1_5 */
	gpio_request(EXYNOS5410_GPX1(5), "GPX1");

	gpio_direction_output(EXYNOS5410_GPX1(5), 1);
	msleep(20);

	gpio_free(EXYNOS5410_GPX1(5));
}

static void s5p_dp_backlight_off(void)
{
	/* LED_BACKLIGHT_RESET: GPX1_5 */
	gpio_request(EXYNOS5410_GPX1(5), "GPX1");

	gpio_direction_output(EXYNOS5410_GPX1(5), 0);
	msleep(20);

	gpio_free(EXYNOS5410_GPX1(5));
}

static struct s5p_dp_platdata universal5410_dp_data __initdata = {
	.video_info	= &universal5410_dp_config,
	.phy_init	= s5p_dp_phy_init,
	.phy_exit	= s5p_dp_phy_exit,
	.backlight_on	= s5p_dp_backlight_on,
	.backlight_off	= s5p_dp_backlight_off,
};
#endif

#ifdef CONFIG_FB_S5P_EXTDSP
static struct s3c_fb_pd_win default_extdsp_data = {
	.width = 1920,
	.height = 1080,
	.default_bpp = 32,
};
#endif

static struct platform_device *universal5410_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&universal5410_mipi_lcd,
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_S5P_DP
	&s5p_device_dp,
	&universal5410_dp_lcd,
#endif

#ifdef CONFIG_FB_S5P_EXTDSP
	&s5p_device_extdsp,
#endif
};

void __init exynos5_universal5410_display_init(void)
{
#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
#ifdef CONFIG_S5P_DP
	s5p_dp_set_platdata(&universal5410_dp_data);
#endif

	s5p_fimd1_set_platdata(&universal5410_lcd1_pdata);
#if !defined(CONFIG_MACH_UNIVERSAL5410)
	samsung_bl_set(&universal5410_bl_gpio_info, &universal5410_bl_data);
#endif

#ifdef CONFIG_FB_S5P_EXTDSP
	s3cfb_extdsp_set_platdata(&default_extdsp_data);
#endif

	platform_add_devices(universal5410_display_devices,
			ARRAY_SIZE(universal5410_display_devices));

#ifdef CONFIG_S5P_DP
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mpll_bpll", 267 * MHZ);
#endif
#ifdef CONFIG_FB_MIPI_DSIM
#ifdef CONFIG_LCD_MIPI_AMS480GYXX
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mpll_bpll", 58 * MHZ);
#elif defined(CONFIG_LCD_MIPI_S6E8FA0) || defined(CONFIG_LCD_MIPI_ER63311)
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mpll_bpll", 134 * MHZ);
#endif
#endif
}
