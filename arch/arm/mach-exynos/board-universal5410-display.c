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
#include <linux/err.h>

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
	bootloaderfb_start = 0; /* disable for copying bootloaderfb */
	return 1;
}
__setup("s3cfb.bootloaderfb=", bootloaderfb_start_setup);

#if defined(CONFIG_LCD_MIPI_S6E8FA0)
static int mipi_power_control(struct mipi_dsim_device *dsim,
				unsigned int power)
{
	if (power) {
		/*mipi 1.8v enable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
		usleep_range(12000, 12000);
	} else {
		/*mipi 1.8v disable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		gpio_free(EXYNOS5410_GPF1(6));
	}

	return 0;
}

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	struct regulator *regulator_1_8;
	struct regulator *regulator_2_8;

	regulator_1_8 = regulator_get(NULL, "vcc_1.8v_lcd");
	regulator_2_8 = regulator_get(NULL, "vcc_2.8v_lcd");

	if (IS_ERR(regulator_1_8))
		printk(KERN_ERR "LCD_1_8: Fail to get regulator!\n");
	if (IS_ERR(regulator_2_8))
		printk(KERN_ERR "LCD_2.8: Fail to get regulator!\n");

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
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		gpio_free(EXYNOS5410_GPJ1(0));
	}
	regulator_put(regulator_1_8);
	regulator_put(regulator_2_8);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{
	/*LCD RESET high*/
	gpio_request_one(EXYNOS5410_GPJ1(0),
	GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
	usleep_range(5000, 6000);
	gpio_set_value(EXYNOS5410_GPJ1(0), 0);
	usleep_range(5000, 6000);
	gpio_set_value(EXYNOS5410_GPJ1(0), 1);
	gpio_free(EXYNOS5410_GPJ1(0));

	usleep_range(12000, 12000);

	return 0;
}

static struct lcd_platform_data s6e8fa0_platform_data = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
};

/*
HBP ->left_margin
HFP ->right_margin
HSP ->hsync_len

VBP ->upper_margin
VFP ->lower_margin
VSW ->vsync_len
*/

#define HBP		16
#define HFP		40
#define HFP_DSIM	(40*109.5/133)
#define HSP		10
#define VBP		3
#define VFP		12
#define VSW		1

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
	.width			= 62,
	.height			= 110,
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
	.width			= 62,
	.height			= 110,
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
	.width			= 62,
	.height			= 110,
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
	.width			= 62,
	.height			= 110,
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
	.width			= 62,
	.height			= 110,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#elif defined(CONFIG_LCD_MIPI_S6E3FA0)
static int mipi_power_control(struct mipi_dsim_device *dsim,
				unsigned int power)
{
	if (power) {
		/*mipi 1.8v enable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5410_GPF1(6));
		usleep_range(12000, 12000);
	} else {
		/*mipi 1.8v disable*/
		gpio_request_one(EXYNOS5410_GPF1(6),
				GPIOF_OUT_INIT_LOW, "GPIO_MIPI_18V_EN");
		gpio_free(EXYNOS5410_GPF1(6));
	}

	return 0;
}

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	struct regulator *regulator_1_8;
	struct regulator *regulator_2_8;

	regulator_1_8 = regulator_get(NULL, "vcc_1.8v_lcd");
	regulator_2_8 = regulator_get(NULL, "vcc_2.8v_lcd");

	if (IS_ERR(regulator_1_8))
		printk(KERN_ERR "LCD_1_8: Fail to get regulator!\n");
	if (IS_ERR(regulator_2_8))
		printk(KERN_ERR "LCD_2.8: Fail to get regulator!\n");

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
		gpio_request_one(EXYNOS5410_GPJ1(0),
				GPIOF_OUT_INIT_LOW, "GPIO_MLCD_RST");
		gpio_free(EXYNOS5410_GPJ1(0));
	}
	regulator_put(regulator_1_8);
	regulator_put(regulator_2_8);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{
	/*LCD RESET high*/
	gpio_request_one(EXYNOS5410_GPJ1(0),
	GPIOF_OUT_INIT_HIGH, "GPIO_MLCD_RST");
	usleep_range(5000, 6000);
	gpio_set_value(EXYNOS5410_GPJ1(0), 0);
	usleep_range(5000, 6000);
	gpio_set_value(EXYNOS5410_GPJ1(0), 1);
	gpio_free(EXYNOS5410_GPJ1(0));

	usleep_range(12000, 12000);

	return 0;
}

static struct lcd_platform_data s6e3fa0_platform_data = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
};

#ifdef CONFIG_FB_I80_COMMAND_MODE
static struct s3c_fb_pd_win universal5410_fb_win0 = {
	.win_mode = {
		.left_margin	= 1,
		.right_margin	= 1,
		.upper_margin	= 1,
		.lower_margin	= 1,
		.hsync_len	= 1,
		.vsync_len	= 1,
		.xres		= 1080,
		.yres		= 1920,
		.cs_setup_time	= 1,
		.wr_setup_time	= 0,
		.wr_act_time	= 1,
		.wr_hold_time	= 0,
		.rs_pol		= 0,
		.i80en		= 1,
	},
	.virtual_x		= 1088,
	.virtual_y		= 1920 * 2,
	.width			= 71,
	.height			= 114,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#else
#define HBP		16
#define HFP		40
#define HFP_DSIM	(40*109.5/133)
#define HSP		10
#define VBP		3
#define VFP		12
#define VSW		1

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
	.width			= 71,
	.height			= 114,
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
	.width			= 71,
	.height			= 114,
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
	.width			= 71,
	.height			= 114,
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
	.width			= 71,
	.height			= 114,
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
	.width			= 71,
	.height			= 114,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif
#endif

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

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

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
#ifdef CONFIG_FB_I80_COMMAND_MODE
	reg |= (1 << 24);	/*i80 interface*/
#else
	reg &= ~(3 << 24);	/*VT_DIP1 - RGB*/
#endif
	reg |= (1 << 23);	/*FIFORST_DISP -1*/
	reg &= ~(1 << 15);	/*FIMDBYPASS_DISP1 -0*/
	reg |= (1 << 14);	/*MIE_DISP1 - MDNIE -1 */
	reg |= (1 << 0);	/*MIE_LBLK1 - MDNIE -1*/
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#else
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
#ifdef CONFIG_FB_I80_COMMAND_MODE
	reg |= (1 << 24);
#endif
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#endif
}

static struct s3c_fb_platdata universal5410_lcd1_pdata __initdata = {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	.win[0]		= &universal5410_fb_win0,
	.win[1]		= &universal5410_fb_win0,
	.win[2]		= &universal5410_fb_win0,
	.win[3]		= &universal5410_fb_win0,
	.win[4]		= &universal5410_fb_win0,
#else
	.win[0]		= &universal5410_fb_win0,
	.win[1]		= &universal5410_fb_win1,
	.win[2]		= &universal5410_fb_win2,
	.win[3]		= &universal5410_fb_win3,
	.win[4]		= &universal5410_fb_win4,
#endif
	.default_win	= 2,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#ifdef CONFIG_FB_MIPI_DSIM
	.vidcon1	= VIDCON1_INV_VCLK,
#else
	.vidcon1	= 0,
#endif
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.ip_version     = EXYNOS5_813,
	.dsim_on        = s5p_mipi_dsi_enable_by_fimd,
	.dsim_off       = s5p_mipi_dsi_disable_by_fimd,
	.dsim1_device   = &s5p_device_mipi_dsim1.dev,
};

#ifdef CONFIG_FB_MIPI_DSIM
#ifdef CONFIG_FB_I80_COMMAND_MODE
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_COMMAND,
	.e_pixel_format = DSIM_24BPP_888,

	.eot_disable	= false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,

	/*896Mbps*/
	.p = 3,
	.m = 56,
	.s = 0,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 8 * MHZ, /* escape clk : 8MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
};

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.lcd_size.width			= 1080,
	.lcd_size.height		= 1920,
	.mipi_ddi_pd			= &s6e3fa0_platform_data,
};
#else
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
#if defined(CONFIG_LCD_MIPI_S6E8FA0) || defined(CONFIG_LCD_MIPI_S6E3FA0)
	.auto_vertical_cnt = false,
	.hfp = true,
	.hbp = false,

	/*896Mbps*/
	.p = 3,
	.m = 56,
	.s = 0,
#endif

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 100000,

	.esc_clk = 8 * MHZ,

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x10,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */
};

/* stable_vfp + cmd_allow <= lower_margin(vfp) */
#if defined(CONFIG_LCD_MIPI_S6E8FA0)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= HBP,
	.rgb_timing.right_margin	= HFP_DSIM,
	.rgb_timing.upper_margin	= VBP,
	.rgb_timing.hsync_len		= HSP,
	.rgb_timing.vsync_len		= VSW,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 6,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= 1080,
	.lcd_size.height		= 1920,
	.mipi_ddi_pd			= &s6e8fa0_platform_data,
};
#endif
#if defined(CONFIG_LCD_MIPI_S6E3FA0)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= HBP,
	.rgb_timing.right_margin	= HFP_DSIM,
	.rgb_timing.upper_margin	= VBP,
	.rgb_timing.hsync_len		= HSP,
	.rgb_timing.vsync_len		= VSW,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 6,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= 1080,
	.lcd_size.height		= 1920,
	.mipi_ddi_pd			= &s6e3fa0_platform_data,
};
#endif
#endif

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_power_control,
	.part_reset		= s5p_dsim_part_reset,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,
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
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_FB_S5P_EXTDSP
	&s5p_device_extdsp,
#endif
};

#if defined(CONFIG_FB_LCD_FREQ_SWITCH)
static unsigned int freq_limit = 40;

struct platform_device lcdfreq_device = {
		.name		= "lcdfreq",
		.id		= -1,
		.dev		= {
			.parent	= &s5p_device_fimd1.dev,
			.platform_data = &freq_limit,
	},
};

static void __init lcdfreq_device_register(void)
{
	int ret;

	ret = platform_device_register(&lcdfreq_device);
	if (ret)
		pr_err("failed to register %s: %d\n", __func__, ret);
}
#endif

void __init exynos5_universal5410_display_init(void)
{
	struct resource *res;

#if defined(CONFIG_LCD_MIPI_S6E8FA0)
	unsigned int lcd_id = lcdtype & 0xF;

	if (!gpio_get_value(GPIO_OLED_ID)) {
		dsim_info.dsim_ddi_pd = &ea8062_mipi_lcd_driver;
		pr_err("panel M\n");
	} else {
		if (lcd_id == 1 || lcd_id == 2) {
			dsim_info.dsim_ddi_pd = &s6e8fa0_mipi_lcd_driver;
			pr_err("panel A,B,C\n");
		} else if (lcd_id == 3 || lcd_id == 4) {
			dsim_info.dsim_ddi_pd = &s6e8fa0_6P_mipi_lcd_driver;
			pr_err("panel D,E,F\n");
		} else if (lcd_id == 5 || lcd_id == 6) {
			dsim_info.dsim_ddi_pd = &s6e8fa0_G_mipi_lcd_driver;
			pr_err("panel G\n");
		} else if (lcd_id == 7) {
			dsim_info.dsim_ddi_pd = &s6e8fa0_I_mipi_lcd_driver;
			pr_err("panel I\n");
		} else if (lcd_id == 8) {
			dsim_info.dsim_ddi_pd = &s6e8fa0_J_mipi_lcd_driver;
			pr_err("panel J\n");
		} else {
			dsim_info.dsim_ddi_pd = &s6e8fa0_I_mipi_lcd_driver;
			pr_err("panel select fail\n");
		}
	}
#elif defined(CONFIG_LCD_MIPI_S6E3FA0)
	dsim_info.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver;
#endif

#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
	s5p_fimd1_set_platdata(&universal5410_lcd1_pdata);

#ifdef CONFIG_FB_S5P_EXTDSP
	s3cfb_extdsp_set_platdata(&default_extdsp_data);
#endif

	platform_add_devices(universal5410_display_devices,
		ARRAY_SIZE(universal5410_display_devices));

#ifdef CONFIG_FB_MIPI_DSIM
#if defined(CONFIG_LCD_MIPI_S6E8FA0) || defined(CONFIG_LCD_MIPI_S6E3FA0)
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
		"sclk_fimd", "mout_mpll_bpll", 134 * MHZ);
#endif
#endif
#ifdef CONFIG_FB_S5P_MDNIE
	keep_lcd_clk(&s5p_device_fimd1.dev);
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
		"sclk_mdnie", "mout_mpll_bpll", 134 * MHZ);
	mdnie_device_register();
#endif
#if defined(CONFIG_FB_LCD_FREQ_SWITCH)
	lcdfreq_device_register();
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

