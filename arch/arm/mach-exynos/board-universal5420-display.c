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

#ifdef CONFIG_FB_S5P_MDNIE
#include <linux/mdnie.h>
#endif
#include <linux/lcd.h>

unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);
static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);
	return 1;
}
__setup("lcdtype=", lcdtype_setup);

phys_addr_t bootloaderfb_start;
phys_addr_t bootloaderfb_size = 1920 * 1080 * 4;
static int __init bootloaderfb_start_setup(char *str)
{
	get_option(&str, &bootloaderfb_start);
	//bootloaderfb_start = 0; /* disable for copying bootloaderfb */
	return 1;
}
__setup("s3cfb.bootloaderfb=", bootloaderfb_start_setup);

#if defined(CONFIG_LCD_MIPI_S6E8AA0)
static void mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	if (power) {
		/* Reset */
		gpio_request_one(EXYNOS5420_GPJ4(3),
				GPIOF_OUT_INIT_HIGH, "GPJ4");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPJ4(3), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPJ4(3), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPJ4(3));
		/* Power */
		gpio_request_one(EXYNOS5420_GPH0(0),
				GPIOF_OUT_INIT_HIGH, "GPH0");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPH0(0));
	} else {
		/* Reset */
		gpio_request_one(EXYNOS5420_GPJ4(3),
				GPIOF_OUT_INIT_LOW, "GPJ4");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPJ4(3));
		/* Power */
		gpio_request_one(EXYNOS5420_GPH0(0),
				GPIOF_OUT_INIT_LOW, "GPH0");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPH0(0));
	}
}

#define SMDK5420_HBP (11)
#define SMDK5420_HFP (11)
#define SMDK5420_HFP_DSIM (11)
#define SMDK5420_HSP (2)
#define SMDK5420_VBP (3)
#define SMDK5420_VFP (3)
#define SMDK5420_VSW (2)
#define SMDK5420_XRES (800)
#define SMDK5420_YRES (1280)
#define SMDK5420_VIRTUAL_X (800)
#define SMDK5420_VIRTUAL_Y (1280 * 2)
#define SMDK5420_WIDTH (71)
#define SMDK5420_HEIGHT (114)
#define SMDK5420_MAX_BPP (32)
#define SMDK5420_DEFAULT_BPP (24)

#elif defined(CONFIG_S5P_DP)
static void dp_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* PMIC_LDO_19_EN */
	gpio_request(EXYNOS5420_GPJ3(6), "GPJ3");
	if (power)
		gpio_direction_output(EXYNOS5420_GPJ3(6), 1);

#ifndef CONFIG_BACKLIGHT_PWM
	/* LCD_PWM_IN_2.8V: LCD_B_PWM, GPB2_0 */
	gpio_request(EXYNOS5420_GPB2(0), "GPB2");
#endif

	/* LCD_EN: GPH0_1 */
	gpio_request(EXYNOS5420_GPH0(1), "GPH0");

	/* LCD_EN: GPH0_1 */
	gpio_direction_output(EXYNOS5420_GPH0(1), power);
	msleep(90);

#ifndef CONFIG_BACKLIGHT_PWM
	/* LCD_PWM_IN_2.8V: LCD_B_PWM, GPB2_0 */
	gpio_direction_output(EXYNOS5420_GPB2(0), power);

	gpio_free(EXYNOS5420_GPB2(0));
#endif

	/* PMIC_LDO_19_EN */
	if (!power)
		gpio_direction_output(EXYNOS5420_GPJ3(6), 0);

	gpio_free(EXYNOS5420_GPH0(1));
	gpio_free(EXYNOS5420_GPJ3(6));
}

static struct plat_lcd_data universal5420_dp_lcd_data = {
	.set_power	= dp_lcd_set_power,
};

static struct platform_device universal5420_dp_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &universal5420_dp_lcd_data,
	},
};

#define SMDK5420_HBP 80
#define SMDK5420_HFP 48
#define SMDK5420_HFP_DSIM (40*109.5/133)
#define SMDK5420_HSP 32
#define SMDK5420_VBP 37
#define SMDK5420_VFP 3
#define SMDK5420_VSW 6
#define SMDK5420_XRES 2560
#define SMDK5420_YRES 1600
#define SMDK5420_VIRTUAL_X (2560)
#define SMDK5420_VIRTUAL_Y (1640 * 2)
#define SMDK5420_WIDTH 59
#define SMDK5420_HEIGHT 104
#define SMDK5420_MAX_BPP 32
#define SMDK5420_DEFAULT_BPP 24

#elif defined(CONFIG_LCD_MIPI_S6E8FA0)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	if (power) {
		/* mipi 1.8v enable */
		gpio_request_one(EXYNOS5420_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPF1(6));

		gpio_request_one(EXYNOS5420_GPY7(4),
				GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPY7(4), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPY7(4), 1);
		gpio_free(EXYNOS5420_GPY7(4));
		usleep_range(7000, 8000);
	} else {
		/* LCD RESET low */
		gpio_request_one(EXYNOS5420_GPY7(4),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPY7(4));

		gpio_request_one(EXYNOS5420_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPF1(6));
	}
}

static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
				unsigned int power)
{
	if (power) {
		/* mipi 1.8v enable */
		gpio_request_one(EXYNOS5420_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPF1(6));

		gpio_request_one(EXYNOS5420_GPY7(4),
				GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPY7(4), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPY7(4), 1);
		gpio_free(EXYNOS5420_GPY7(4));
		usleep_range(7000, 8000);
	} else {
		/* LCD RESET low */
		gpio_request_one(EXYNOS5420_GPY7(4),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPY7(4));

		gpio_request_one(EXYNOS5420_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPF1(6));
	}
	return 0;
}

static struct plat_lcd_data universal5420_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device universal5420_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &universal5420_mipi_lcd_data,
};

#define SMDK5420_HBP 16
#define SMDK5420_HFP 40
#define SMDK5420_HFP_DSIM (40*109.5/133)
#define SMDK5420_HSP 10
#define SMDK5420_VBP 1
#define SMDK5420_VFP 13
#define SMDK5420_VSW 2
#define SMDK5420_XRES 1080
#define SMDK5420_YRES 1920
#define SMDK5420_VIRTUAL_X (1088)
#define SMDK5420_VIRTUAL_Y (1920*2)
#define SMDK5420_WIDTH 59
#define SMDK5420_HEIGHT 104
#define SMDK5420_MAX_BPP 32
#define SMDK5420_DEFAULT_BPP 24

#elif defined(CONFIG_LCD_MIPI_S6E3FA0)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
				unsigned int power)
{
	if (power) {
		/* mipi 1.8v enable */
		gpio_request_one(EXYNOS5420_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPF1(6));
	} else {
		gpio_request_one(EXYNOS5420_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPF1(6));
	}
	return 0;
}

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	struct regulator *regulator_1_8;
	struct regulator *regulator_2_8;

	regulator_1_8 = regulator_get(NULL, "vcc_1.8v_lcd");
	regulator_2_8 = regulator_get(NULL, "vcc_3.0v_lcd");

	if (IS_ERR(regulator_1_8))
		printk(KERN_ERR "LCD_1_8: Fail to get regulator!\n");
	if (IS_ERR(regulator_2_8))
		printk(KERN_ERR "LCD_3.0: Fail to get regulator!\n");

	if (enable) {
		/*panel power enable*/
		regulator_enable(regulator_2_8);
		usleep_range(5000, 8000);
		regulator_enable(regulator_1_8);
	} else {
		/*lcd 1.8V disable*/
		if (regulator_is_enabled(regulator_1_8))
				regulator_disable(regulator_1_8);
		if (regulator_is_enabled(regulator_2_8))
				regulator_disable(regulator_2_8);

		/*LCD RESET low*/
		gpio_request_one(EXYNOS5420_GPY7(4),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		gpio_free(EXYNOS5420_GPY7(4));
	}
	regulator_put(regulator_1_8);
	regulator_put(regulator_2_8);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{
	/*LCD RESET high*/
	gpio_request_one(EXYNOS5420_GPY7(4),
	GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
	usleep_range(5000, 6000);
	gpio_set_value(EXYNOS5420_GPY7(4), 0);
	usleep_range(5000, 6000);
	gpio_set_value(EXYNOS5420_GPY7(4), 1);
	gpio_free(EXYNOS5420_GPY7(4));

	usleep_range(12000, 12000);

	return 0;
}

static struct lcd_platform_data s6e3fa0_platform_data = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
};

#ifdef CONFIG_FB_I80IF
#define SMDK5420_HBP 2
#define SMDK5420_HFP 2
#define SMDK5420_HFP_DSIM 1
#define SMDK5420_HSP 1
#define SMDK5420_VBP 4
#define SMDK5420_VFP 1
#define SMDK5420_VSW 1
#else
#define SMDK5420_HBP 16
#define SMDK5420_HFP 40
#define SMDK5420_HFP_DSIM (40*109.5/133)
#define SMDK5420_HSP 10
#define SMDK5420_VBP 1
#define SMDK5420_VFP 13
#define SMDK5420_VSW 2
#endif
#define SMDK5420_XRES 1080
#define SMDK5420_YRES 1920
#define SMDK5420_VIRTUAL_X (1088)
#define SMDK5420_VIRTUAL_Y (1920*2)
#define SMDK5420_WIDTH 71
#define SMDK5420_HEIGHT 126
#define SMDK5420_MAX_BPP 32
#define SMDK5420_DEFAULT_BPP 24

#ifdef CONFIG_FB_S5P_MDNIE
static struct platform_mdnie_data mdnie_data = {
	.display_type	= -1,
};

struct platform_device mdnie_device = {
		.name		 = "mdnie",
		.id	 = -1,
		.dev		 = {
			.parent		= &s5p_device_fimd1.dev,
			.platform_data = &mdnie_data,
	},
};

static void __init mdnie_device_register(void)
{
	int ret;

	ret = platform_device_register(&mdnie_device);
	if (ret)
		printk(KERN_ERR "failed to register mdnie device: %d\n",
				ret);
}

/* Keep on clock of FIMD during boot time  */
static int keep_lcd_clk(struct device *dev)
{
	struct clk *lcd_clk;

	lcd_clk = clk_get(dev, "lcd");
	if (IS_ERR(lcd_clk)) {
		pr_err("failed to get fimd clock for keep screen on\n");
		return PTR_ERR(lcd_clk);
	}
	clk_enable(lcd_clk);
	return 0;
}
#endif
#endif

static struct s3c_fb_pd_win universal5420_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK5420_HBP,
		.right_margin	= SMDK5420_HFP,
		.upper_margin	= SMDK5420_VBP,
		.lower_margin	= SMDK5420_VFP,
		.hsync_len	= SMDK5420_HSP,
		.vsync_len	= SMDK5420_VSW,
		.xres		= SMDK5420_XRES,
		.yres		= SMDK5420_YRES,
#if defined(CONFIG_FB_I80IF) && !defined(CONFIG_FB_S5P_MDNIE)
		.cs_setup_time	= 1,
		.wr_setup_time	= 0,
		.wr_act_time	= 1,
		.wr_hold_time	= 0,
		.rs_pol		= 0,
		.i80en		= 1,
#endif
	},
	.virtual_x		= SMDK5420_VIRTUAL_X,
	.virtual_y		= SMDK5420_VIRTUAL_Y,
	.width			= SMDK5420_WIDTH,
	.height			= SMDK5420_HEIGHT,
	.max_bpp		= SMDK5420_MAX_BPP,
	.default_bpp		= SMDK5420_DEFAULT_BPP,
};

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

#if defined(CONFIG_S5P_DP)
	/* Set Hotplug detect for DP */
	gpio_request(EXYNOS5420_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5420_GPX0(7), S3C_GPIO_SFN(3));
#endif
#ifdef CONFIG_FB_I80IF
	gpio_request(EXYNOS5420_GPJ4(0), "GPJ4");
	s3c_gpio_cfgpin(EXYNOS5420_GPJ4(0), S3C_GPIO_SFN(2));
#endif
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */

#ifdef CONFIG_FB_S5P_MDNIE
	/* SYSREG SETTING */
	reg = 0;
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1>>23); /*FIFO software Reset*/
	__raw_writel(reg, S3C_VA_SYS + 0x0214);

	reg &= ~(7 << 29);	/*sync*/
	reg &= ~(1 << 27);	/*DISP0_SRC not userd*/
	reg &= ~(3 << 24);	/*VT_DIP1 - RGB*/
	reg |= (1 << 23);	/*FIFORST_DISP -1*/
	reg &= ~(1 << 15);	/*FIMDBYPASS_DISP1 -0*/
	reg |= (1 << 14);	/*MIE_DISP1 - MDNIE -1 */
	reg |= (1 << 0);	/*MIE_LBLK1 - MDNIE -1*/
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#else
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#endif

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
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * MIC of DISP1_BLK Bypass selection: DISP1BLK_CFG[11]
	 * --------------------
	 *  0 | MIC
	 *  1 | Bypass : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg |= (1 << 11);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
}

static struct s3c_fb_platdata universal5420_lcd1_pdata __initdata = {
	.win[0]		= &universal5420_fb_win0,
	.win[1]		= &universal5420_fb_win0,
	.win[2]		= &universal5420_fb_win0,
	.win[3]		= &universal5420_fb_win0,
	.win[4]		= &universal5420_fb_win0,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_S5P_DP)
	.vidcon1	= 0,
#else
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.ip_version	= EXYNOS5_813,
	.dsim_on	= s5p_mipi_dsi_enable_by_fimd,
	.dsim_off	= s5p_mipi_dsi_disable_by_fimd,
	.dsim_clk_on	= s5p_mipi_dsi_clk_enable_by_fimd,
	.dsim_clk_off	= s5p_mipi_dsi_clk_disable_by_fimd,
#ifdef CONFIG_FB_I80IF
	.dsim_get_state = s5p_mipi_dsi_get_mipi_state,
#endif
	.dsim1_device   = &s5p_device_mipi_dsim1.dev,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN SMDK5420_HBP
#define DSIM_R_MARGIN SMDK5420_HFP_DSIM
#define DSIM_UP_MARGIN SMDK5420_VBP
#define DSIM_LOW_MARGIN SMDK5420_VFP
#define DSIM_HSYNC_LEN SMDK5420_HSP
#define DSIM_VSYNC_LEN SMDK5420_VSW
#define DSIM_WIDTH SMDK5420_XRES
#define DSIM_HEIGHT SMDK5420_YRES

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= DSIM_L_MARGIN,
	.rgb_timing.right_margin	= DSIM_R_MARGIN,
	.rgb_timing.upper_margin	= DSIM_UP_MARGIN,
	.rgb_timing.lower_margin	= DSIM_LOW_MARGIN,
	.rgb_timing.hsync_len		= DSIM_HSYNC_LEN,
	.rgb_timing.vsync_len		= DSIM_VSYNC_LEN,
	.rgb_timing.stable_vfp		= 3,
	.rgb_timing.cmd_allow		= 1,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
	.mipi_ddi_pd			= &s6e3fa0_platform_data,
};

#if defined(CONFIG_LCD_MIPI_S6E8AA0)
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= false,
	.auto_vertical_cnt = true,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 4,
	.m = 80,
	.s = 2,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 7 * 1000000, /* escape clk : 7MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e8aa0_mipi_lcd_driver,
};
#elif defined(CONFIG_LCD_MIPI_S6E8FA0)
static struct mipi_dsim_config dsim_info = {
	.e_interface = DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush = false,
	.eot_disable = true,
	.auto_vertical_cnt = false,
	.hse = false,
	.hfp = true,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk = DSIM_PLL_OUT_DIV8,
	.e_burst_mode = DSIM_BURST,

	.p = 4,
	.m = 73,
	.s = 0,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 7 * 1000000, /* escape clk : 7MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x01,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e8fa0_6P_mipi_lcd_driver,
};
#elif defined(CONFIG_LCD_MIPI_S6E3FA0)
#ifdef CONFIG_FB_I80IF
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_COMMAND,
	.e_pixel_format = DSIM_24BPP_888,

	.eot_disable	= true,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,

	/*1092Mbps*/
	.p = 4,
	.m = 91,
	.s = 0,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 8 * MHZ, /* escape clk : 8MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x10,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
};
#else
static struct mipi_dsim_config dsim_info = {
	.e_interface = DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush = false,
	.eot_disable = true,
	.auto_vertical_cnt = false,
	.hse = false,
	.hfp = true,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk = DSIM_PLL_OUT_DIV8,
	.e_burst_mode = DSIM_BURST,

	.p = 3,
	.m = 56,
	.s = 0,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 7 * 1000000, /* escape clk : 7MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x01,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
};
#endif
#endif

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_lcd_power_control,
	.part_reset		= NULL,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,

	/*
	 * The stable time of needing to write data on SFR
	 * when the mipi mode becomes LP mode.
	 */
	.delay_for_stabilization = 600,
#if defined(CONFIG_FB_I80IF)
	.trigger_set = s3c_fb_enable_trigger_by_dsim,
	.fimd1_device = &s5p_device_fimd1.dev,
#endif
};
#endif

#ifdef CONFIG_S5P_DP
static struct video_info universal5420_dp_config = {
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
	gpio_request(EXYNOS5420_GPX1(5), "GPX1");

	gpio_direction_output(EXYNOS5420_GPX1(5), 1);
	usleep_range(20000, 21000);

	gpio_free(EXYNOS5420_GPX1(5));
}

static void s5p_dp_backlight_off(void)
{
	/* LED_BACKLIGHT_RESET: GPX1_5 */
	gpio_request(EXYNOS5420_GPX1(5), "GPX1");

	gpio_direction_output(EXYNOS5420_GPX1(5), 0);
	usleep_range(20000, 21000);

	gpio_free(EXYNOS5420_GPX1(5));
}

static struct s5p_dp_platdata universal5420_dp_data __initdata = {
	.video_info	= &universal5420_dp_config,
	.phy_init	= s5p_dp_phy_init,
	.phy_exit	= s5p_dp_phy_exit,
	.backlight_on	= s5p_dp_backlight_on,
	.backlight_off	= s5p_dp_backlight_off,
};
#endif

static struct platform_device *universal5420_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_S5P_DP
	&s5p_device_dp,
	&universal5420_dp_lcd,
#endif
};

#ifdef CONFIG_BACKLIGHT_PWM
/* LCD Backlight data */
static struct samsung_bl_gpio_info universal5420_bl_gpio_info = {
	.no = EXYNOS5420_GPB2(0),
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data universal5420_bl_data = {
	.pwm_id = 0,
	.pwm_period_ns = 30000,
};
#endif

void __init exynos5_universal5420_display_init(void)
{
	struct resource *res;
	struct clk *mout_mdnie1;
	struct clk *mout_mpll;

#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
#ifdef CONFIG_S5P_DP
	s5p_dp_set_platdata(&universal5420_dp_data);
#endif
	s5p_fimd1_set_platdata(&universal5420_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&universal5420_bl_gpio_info, &smdk5420_bl_data);
#endif
	platform_add_devices(universal5420_display_devices,
			ARRAY_SIZE(universal5420_display_devices));
#ifdef CONFIG_S5P_DP
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mpll_bpll", 267 * MHZ);
#endif
	mout_mdnie1 = clk_get(NULL, "mout_mdnie1");
	if ((IS_ERR(mout_mdnie1)))
		pr_err("Can't get clock[%s]\n", "mout_mdnie1");

	mout_mpll = clk_get(NULL, "mout_mpll");
	if ((IS_ERR(mout_mpll)))
		pr_err("Can't get clock[%s]\n", "mout_mpll");

	if (mout_mdnie1 && mout_mpll)
		clk_set_parent(mout_mdnie1, mout_mpll);

	if (mout_mdnie1)
		clk_put(mout_mdnie1);
	if (mout_mpll)
		clk_put(mout_mpll);

#ifdef CONFIG_FB_MIPI_DSIM
#if defined(CONFIG_LCD_MIPI_S6E8FA0) || defined(CONFIG_LCD_MIPI_S6E3FA0)
#if defined(CONFIG_FB_I80IF) && !defined(CONFIG_FB_S5P_MDNIE)
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mdnie1", 266 * MHZ);
#else
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mdnie1", 133 * MHZ);
#endif
#else
	/* RPLL rate is 300Mhz, 300/5=60Hz */
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_rpll", 67 * MHZ);
#endif
#endif
#ifdef CONFIG_FB_S5P_MDNIE
	keep_lcd_clk(&s5p_device_fimd1.dev);
#if defined(CONFIG_LCD_MIPI_S6E8FA0) || defined(CONFIG_LCD_MIPI_S6E3FA0)
#ifdef CONFIG_FB_I80IF
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
		"sclk_mdnie", "mout_mdnie1", 266 * MHZ);
#else
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
		"sclk_mdnie", "mout_mdnie1", 133 * MHZ);
#endif
#endif
	mdnie_device_register();
#endif

	res = platform_get_resource(&s5p_device_fimd1, IORESOURCE_MEM, 1);
	if (res) {
		res->start = bootloaderfb_start;
		res->end = res->start + bootloaderfb_size - 1;
		pr_info("bootloader fb located at %8X-%8X\n", res->start, res->end);
	} else {
		pr_err("failed to find bootloader fb resource\n");
	}
}
