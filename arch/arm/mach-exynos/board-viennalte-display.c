/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/clk.h>

#include <video/platform_lcd.h>
#include <video/s5p-dp.h>

#include <plat/backlight.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/dp.h>
#include <plat/fb.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/regs-fb-v4.h>
#include <plat/tv-core.h>

#include <mach/gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/sysmmu.h>

#include "common.h"

#ifdef CONFIG_FB_S5P_MDNIE
#include <linux/mdnie.h>
#endif

#ifdef CONFIG_BACKLIGHT_LP8556
#include <linux/lp8556.h>
#define BLIC_I2C_ID 26
#endif

#ifdef CONFIG_TCON_S6TNDR3X
#include <linux/s6tndr3x_tcon.h>
#define IMPROVE_BL_FLICKER
#define TCON_I2C_ID 27
#endif

#define BLIC_LP8556             0x01
#define BLIC_LP8558_UNMASK      0x02
#define BLIC_LP8558_MASK        0x03

#define LCD_TYPE_PENTILE         0x01
#define LCD_TYPE_REAL            0x02


#define LCD_POWER_OFF_TIME_US   (500 * USEC_PER_MSEC)

#define GPIO_LCD_PWM0_IN_18V    EXYNOS5410_GPB2(0)

#if 0
extern phys_addr_t vienna_bootloader_fb_start;
extern phys_addr_t vienna_bootloader_fb_size;
#endif

phys_addr_t bootloaderfb_start;
phys_addr_t bootloaderfb_size;

EXPORT_SYMBOL(bootloaderfb_start);
EXPORT_SYMBOL(bootloaderfb_size);

static unsigned int brightness_table[256] = {
	0,
};

static unsigned int brightness_level[] = {
	60, 130, 255
};

static unsigned int pwm_level[] = {
	34, 44, 100,
};

static ktime_t lcd_on_time;

static int lcdtype;

static int boot_param_lcdtype(char *str)
{
	get_option(&str, &lcdtype);
	return lcdtype;
}
__setup("lcdtype=", boot_param_lcdtype);


static int get_lcd_type(void)
{
	return lcdtype & 0x0f;
}

static int get_bl_type(void)
{
	return (lcdtype >> 0x4) & 0x0f;
}

#ifdef CONFIG_BACKLIGHT_LP8556
extern int lp8556_init_registers(void);
#endif


#ifdef IMPROVE_BL_FLICKER

#define MAX_BRIGHTNESS 1000000
#define GET_RATIO_ROUND(br, max) (((br * 1000) / max + 5) / 10)

extern int s6tndr3x_tune_decay(unsigned char ratio);

static unsigned char bl_threshold_level[] = {
	20, 30, 40, 100
};

static struct tcon_val tcon_mnbl_100[] = {
	{0x05, 0x32, 0xff},
	{0x05, 0x34, 0x01},
	{0x05, 0x35, 0x01},
	{0x05, 0x36, 0x00},
	{0x05, 0x37, 0x00},
	{0xff, 0xff, 0xff}
};

static struct tcon_val tcon_mnbl_85[] = {
	{0x05, 0x32, 0xd8},
	{0x05, 0x34, 0x01},
	{0x05, 0x35, 0x01},
	{0x05, 0x36, 0x00},
	{0x05, 0x37, 0x00},
	{0xff, 0xff, 0xff}
};

static struct tcon_val tcon_mnbl_70[] = {
	{0x05, 0x32, 0xb2},
	{0x05, 0x34, 0x01},
	{0x05, 0x35, 0x01},
	{0x05, 0x36, 0x00},
	{0x05, 0x37, 0x00},
	{0xff, 0xff, 0xff}
};

static struct tcon_val tcon_mnbl_65[] = {
	{0x05, 0x32, 0xa5},
	{0x05, 0x34, 0x01},
	{0x05, 0x35, 0x01},
	{0x05, 0x36, 0x24},
	{0x05, 0x37, 0x24},
	{0xff, 0xff, 0xff}
};

struct tcon_val *tcon_tune_val[] = {
	tcon_mnbl_100,
	tcon_mnbl_85,
	tcon_mnbl_70,
	tcon_mnbl_65,
};
#endif


#ifdef CONFIG_TCON_S6TNDR3X
struct s6tndr3x_priv_data v1_tcon_pdata = {
	.tcon_ready = 1,
#ifdef IMPROVE_BL_FLICKER
	.decay_cnt = 4,
	.decay_level = bl_threshold_level,
	.decay_tune_val = tcon_tune_val,
	.max_br = MAX_BRIGHTNESS,
#endif
};

static struct i2c_board_info __initdata s6tndr3x_i2c_info[] = {
	{
		I2C_BOARD_INFO("s6tndr3-tcon", S6TNDR3X_I2C_SLAVEADDR),
		.platform_data = &v1_tcon_pdata,
	},
};

static struct i2c_gpio_platform_data s6tndr3x_i2c_platdata = {
	.sda_pin    = GPIO_LCDP_SDA,
	.scl_pin    = GPIO_LCDP_SCL,
	.udelay     = 4, /*default is 250Khz*/
	.sda_is_open_drain  = 0,
	.scl_is_open_drain  = 0,
	.scl_is_output_only = 0,
};

struct platform_device platform_s6tndr3x_i2c = {
	.name   = "i2c-gpio",
	.id     = TCON_I2C_ID,
	.dev.platform_data = &s6tndr3x_i2c_platdata,
};
#endif


int resume_decay_bl;
int lcd_on;
static void vienna_lcd_on(void)
{
	s64 us = ktime_us_delta(lcd_on_time, ktime_get_boottime());
	if (us > LCD_POWER_OFF_TIME_US) {
		pr_warn("lcd on sleep time too long\n");
		us = LCD_POWER_OFF_TIME_US;
	}
	if (us > 0)
		usleep_range(us, us);

	gpio_set_value(GPIO_LCD_EN, 1);
	usleep_range(200000, 200000);

}

static void vienna_lcd_off(void)
{
	gpio_set_value(GPIO_LCD_EN, 0);

	lcd_on_time = ktime_add_us(ktime_get_boottime(), LCD_POWER_OFF_TIME_US);
}

static void vienna_backlight_on(void)
{
	int ret;

	printk(KERN_INFO "%s was called\n", __func__);
#ifdef CONFIG_TCON_S6TNDR3X
	if (get_lcd_type() == LCD_TYPE_PENTILE) {
		s6tndr3x_tune(1);
#ifdef IMPROVE_BL_FLICKER
		if (resume_decay_bl) {
			ret = s6tndr3x_tune_decay(GET_RATIO_ROUND(resume_decay_bl, MAX_BRIGHTNESS));
			if (ret)
				pr_err("[S6TNDR3-TON] failed to tune decay value.. \n");
			resume_decay_bl = 0;
		}
#endif
	}
#endif
	if (get_bl_type() == BLIC_LP8556) {
		gpio_set_value(GPIO_LED_BL_1, 1);
		usleep_range(10000, 10000);
	}
	gpio_set_value(GPIO_LED_BL_RST, 1);

	ret = lp8556_init_registers();
	if (ret)
		printk(KERN_ERR "[DISPLAY:ERR] lp8556_init error(%d)\n", ret);

	lcd_on = 1;
}

static void vienna_backlight_off(void)
{
	printk(KERN_INFO "%s was called\n", __func__);
	gpio_set_value(GPIO_LED_BL_RST, 0);
	if (get_bl_type() == BLIC_LP8556)
		gpio_set_value(GPIO_LED_BL_1, 0);
#ifdef CONFIG_TCON_S6TNDR3X
	if (get_lcd_type() == LCD_TYPE_PENTILE)
		s6tndr3x_tune(0);
#endif
	lcd_on = 0;
}

static void vienna_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	if (power)
		vienna_lcd_on();
	else
		vienna_lcd_off();
}

static struct plat_lcd_data vienna_lcd_data = {
	.set_power	= vienna_lcd_set_power,
};

static struct platform_device vienna_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &vienna_lcd_data,
	},
};

static void vienna_fimd_gpio_setup_24bpp(void)
{
	u32 reg;
	/* Set Hotplug detect for DP */
	gpio_request(EXYNOS5410_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5410_GPX0(7), S3C_GPIO_SFN(3));

#ifdef CONFIG_FB_S5P_MDNIE
	/* SYSREG SETTING */
	reg = 0;
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1<<23); /*FIFO software Reset*/
	__raw_writel(reg, S3C_VA_SYS + 0x0214);

	reg &= ~(7 << 29);  /*sync*/
	reg &= ~(1 << 27);  /*DISP0_SRC not userd*/
	/*reg |= (1 << 28); */
	reg &= ~(3 << 24);  /*VT_DIP1 - RGB*/
	reg |= (1 << 23);   /*FIFORST_DISP -1*/
	reg &= ~(1 << 15);  /*FIMDBYPASS_DISP1 -0*/
	reg |= (1 << 14);   /*MIE_DISP1 - MDNIE -1 */
	reg |= (1 << 0);    /*MIE_LBLK1 - MDNIE -1*/
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
	 /* set ACLK_200 SRC to MPLL(532) */
	reg = readl(EXYNOS5_CLKSRC_TOP0);
	reg &= ~(1 << 12);
	reg |= (0 << 12);
	writel(reg, EXYNOS5_CLKSRC_TOP0);

	/* set 532/(2+1) = 177Mhz */
	reg = readl(EXYNOS5_CLKDIV_TOP0);
	reg &= ~(7 << 12);
	reg |= (1 << 12);
	writel(reg, EXYNOS5_CLKDIV_TOP0);

	reg = __raw_readl(EXYNOS5_CLKSRC_TOP3);
	reg |= (0x01 << 4);
	__raw_writel(reg, EXYNOS5_CLKSRC_TOP3);

#else
	/* basic fimd init */
	exynos5_fimd1_gpio_setup_24bpp();
#endif

	/* Reference clcok selection for DPTX_PHY: pad_osc_clk_24M */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg &= ~(1 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* DPTX_PHY: XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg &= ~(1 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);
}

static struct s3c_fb_pd_win vienna_fb_win2 = {
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
	.width			= 273,
	.height			= 177,
};

static struct s3c_fb_platdata vienna_lcd1_pdata __initdata = {
	.win[0]		= &vienna_fb_win2,
	.win[1]		= &vienna_fb_win2,
	.win[2]		= &vienna_fb_win2,
	.win[3]		= &vienna_fb_win2,
	.win[4]		= &vienna_fb_win2,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= 0,
	.ip_version	= EXYNOS5_813,
	.setup_gpio	= vienna_fimd_gpio_setup_24bpp,
	.backlight_off  = vienna_backlight_off,
	.lcd_off	= vienna_lcd_off,
};

static struct video_info vienna_dp_config = {
	.name			= "WQXGA(2560x1600) LCD",

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

static struct s5p_dp_platdata vienna_dp_data __initdata = {
	.video_info     = &vienna_dp_config,
	.phy_init       = s5p_dp_phy_init,
	.phy_exit       = s5p_dp_phy_exit,
	.backlight_on   = vienna_backlight_on,
	.backlight_off  = vienna_backlight_off,
	.lcd_on		= vienna_lcd_on,
	.lcd_off	= vienna_lcd_off,
};

#if defined (CONFIG_BACKLIGHT_PWM)
/* LCD Backlight data */
static struct samsung_bl_gpio_info vienna_bl_gpio_info = {
	.no	= GPIO_LCD_PWM_IN_18V,
	.func	= S3C_GPIO_SFN(2),
};

static int vienna_bl_notify(struct device *dev, int brightness)
{
	int bl;
	int ret;

	bl = brightness_table[brightness];

#ifdef IMPROVE_BL_FLICKER
	if ((get_lcd_type() == LCD_TYPE_PENTILE) && (lcd_on == 1))
		ret = s6tndr3x_tune_decay(GET_RATIO_ROUND(bl, MAX_BRIGHTNESS));
	else
		resume_decay_bl = bl;
#endif
	return bl;
}

static struct platform_pwm_backlight_data vienna_bl_data = {
#ifdef CONFIG_V1_LTE_REV00
	.pwm_id	= 2,
#else
	.pwm_id = 1,
#endif
	.pwm_period_ns	= 1000000,
	.dft_brightness = 100,
	.fdft_brightness = 130,
	.pwm_requested = 44,
	.max_brightness = 255,
	.notify = vienna_bl_notify,
};
#endif

#ifdef CONFIG_FB_S5P_MDNIE
/* minwoo7945.kim */

#if defined (CONFIG_S5P_MDNIE_PWM)
int mdnie_pwm_br[256] = {0, };
#endif

static struct platform_mdnie_data mdnie_data = {
	.display_type	= -1,
	.support_pwm    = 0,
#if defined (CONFIG_S5P_MDNIE_PWM)
	.pwm_out_no = GPIO_MDNIE_PWM,
	.pwm_out_func = 3,
	.name = "panel",
	.br_table = mdnie_pwm_br,
	.dft_bl = 103,
#endif
};

struct platform_device mdnie_device = {
	.name = "mdnie",
	.id	 = -1,
	.dev = {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data = &mdnie_data,
	},
};
#endif


#ifdef CONFIG_BACKLIGHT_LP8556

struct lp8556_rom_data lp8556_default_rom[] = {
	{ .addr = 0x01, .val = 0x80, },
	{ .addr = 0xa3, .val = 0x33, },
	{ .addr = 0xa5, .val = 0x14, },
};

struct lp8556_platform_data lp8556_default_pdata = {
	.load_new_rom_data = 1,
	.size_program = ARRAY_SIZE(lp8556_default_rom),
	.rom_data = lp8556_default_rom,
};

static struct i2c_board_info __initdata lp8556_i2c_info[] = {
	{
		I2C_BOARD_INFO("lp8556", 0x2C),
		.platform_data = &lp8556_default_pdata,
	},
};

struct lp8556_rom_data lp8558_unmask_pentile_data[] = {
	{.addr = 0x01, .val = 0x80},
	{.addr = 0xa0, .val = 0x82},
	{.addr = 0xa1, .val = 0x6e},
	{.addr = 0x98, .val = 0xa1},
	{.addr = 0x9e, .val = 0x21},
	{.addr = 0xa2, .val = 0x28},
	{.addr = 0xa3, .val = 0x02},
	{.addr = 0xa4, .val = 0x72},
	{.addr = 0xa5, .val = 0x14},
	{.addr = 0xa6, .val = 0x40},
	{.addr = 0xa7, .val = 0xfb},
	{.addr = 0xa8, .val = 0x00},
	{.addr = 0xa9, .val = 0xa0},
	{.addr = 0xaa, .val = 0x0f},
	{.addr = 0xab, .val = 0x00},
	{.addr = 0xac, .val = 0x00},
	{.addr = 0xad, .val = 0x00},
	{.addr = 0xae, .val = 0x0e},
	{.addr = 0xaf, .val = 0x01},
};

struct lp8556_platform_data lp8558_unmask_pentile = {
	.load_new_rom_data = 1,
	.size_program = ARRAY_SIZE(lp8558_unmask_pentile_data),
	.rom_data = lp8558_unmask_pentile_data,
};

struct lp8556_rom_data lp8558_unmask_real_data[] = {
	{.addr = 0x01, .val = 0x80},
	{.addr = 0xa0, .val = 0x33},
	{.addr = 0xa1, .val = 0x6b},
	{.addr = 0x98, .val = 0xa1},
	{.addr = 0x9e, .val = 0x21},
	{.addr = 0xa2, .val = 0x28},
	{.addr = 0xa3, .val = 0x02},
	{.addr = 0xa4, .val = 0x72},
	{.addr = 0xa5, .val = 0x14},
	{.addr = 0xa6, .val = 0x40},
	{.addr = 0xa7, .val = 0xfb},
	{.addr = 0xa8, .val = 0x00},
	{.addr = 0xa9, .val = 0xa0},
	{.addr = 0xaa, .val = 0x0f},
	{.addr = 0xab, .val = 0x00},
	{.addr = 0xac, .val = 0x00},
	{.addr = 0xad, .val = 0x00},
	{.addr = 0xae, .val = 0x0e},
	{.addr = 0xaf, .val = 0x01},
};

struct lp8556_platform_data lp8558_unmask_real = {
	.load_new_rom_data = 1,
	.size_program = ARRAY_SIZE(lp8558_unmask_real_data),
	.rom_data = lp8558_unmask_real_data,
};

static struct i2c_gpio_platform_data lp8556_i2c_platdata = {
	.sda_pin    = GPIO_DCDC_SDA_18V,
	.scl_pin    = GPIO_DCDC_SCL_18V,
	.udelay     = 4, /*default is 250Khz*/
	.sda_is_open_drain  = 0,
	.scl_is_open_drain  = 0,
	.scl_is_output_only = 0,
};

struct platform_device platform_lp8665_i2c = {
	.name   = "i2c-gpio",
	.id     = BLIC_I2C_ID,
	.dev.platform_data = &lp8556_i2c_platdata,
};
#endif

static struct platform_device *vienna_display_devices[] __initdata = {
	&s5p_device_fimd1,
	&vienna_lcd,
	&s5p_device_dp,
#ifdef CONFIG_FB_S5P_MDNIE
	&mdnie_device,
#endif
#ifdef CONFIG_BACKLIGHT_LP8556
	&platform_lp8665_i2c,
#endif
};

extern int system_rev;


#ifdef CONFIG_S5P_MDNIE_PWM
int exynos5_set_clk_rate_mdnie_pwm(void)
{
	int ret;
	struct clk *ext_clk = NULL;
	struct clk *mout_pwm = NULL;
	struct clk *mdnie_pwm = NULL;

	ext_clk = clk_get(NULL, "ext_xtal");
	if (IS_ERR(ext_clk)) {
		pr_err("Unable to get ext_xtal clock\n");
		ret = PTR_ERR(ext_clk);
		goto set_mdnie_pwm_clk_fail;
	}
	mout_pwm = clk_get(NULL, "dout_mdnie_pwm");
	if (IS_ERR(mout_pwm)) {
		pr_err("Unable to get dout_mdnie_pwm clock\n");
		ret = PTR_ERR(mout_pwm);
		goto set_mdnie_pwm_clk_fail;
	}

	mdnie_pwm = clk_get(NULL, "sclk_mdnie_pwm");
	if (IS_ERR(mdnie_pwm)) {
		pr_err("Unable to get sclk_mdnie_pwm clock\n");
		ret = PTR_ERR(mdnie_pwm);
		goto set_mdnie_pwm_clk_fail;
	}

	ret = clk_set_parent(mout_pwm, ext_clk);
	if (ret) {
		pr_err("Unable to set dout_mdnie_pwm 's parent clock\n");
		goto set_mdnie_pwm_clk_fail;
	}

	ret = clk_set_rate(mout_pwm, 24000000);
	if (ret) {
		pr_err("dout_mdnie_pwm rate change failed\n");
		goto set_mdnie_pwm_clk_fail;
	}

	ret = clk_set_parent(mdnie_pwm, mout_pwm);
	if (ret) {
		pr_err("Unable to set sclk_mdnie_pwm 's parent clock\n");
		goto set_mdnie_pwm_clk_fail;
	}

	/* sclk_mdnie_pwm rate = 12Mhz
	actual mdine_pwm output = 12MHz/2048 = 5.68kHz */
	ret = clk_set_rate(mdnie_pwm, 12000000);
	if (ret) {
		pr_err("dout_mdnie_pwm rate change failed\n");
		goto set_mdnie_pwm_clk_fail;
	}

set_mdnie_pwm_clk_fail:
	if (ext_clk)
		clk_put(ext_clk);
	if (mout_pwm)
		clk_put(mout_pwm);
	if (mdnie_pwm)
		clk_put(mdnie_pwm);

	return ret;
}
#endif


void __init exynos5_universal5410_display_init(void)
{
	int ret;
#if 0
	struct resource *res;
#endif
	int period;
	int i, diff;
	int j = 0;

	gpio_request_one(GPIO_LCD_EN, GPIOF_OUT_INIT_LOW, "LCD_EN");

bl_rst_gpio_request:
	ret = gpio_request_one(GPIO_LED_BL_RST, GPIOF_OUT_INIT_LOW, "LED_BL_RST");
	if (ret) {
		gpio_free(GPIO_LED_BL_RST);
		goto bl_rst_gpio_request;
	}

	if (get_bl_type() == BLIC_LP8556) {
alloc_bl1_gpio:
		ret = gpio_request_one(GPIO_LED_BL_1, GPIOF_OUT_INIT_HIGH, "LED_BL_1");
		if (ret) {
			gpio_free(GPIO_LED_BL_1);
			goto alloc_bl1_gpio;
		}
	}
	/* for pentile lcd */
	if (get_lcd_type() == LCD_TYPE_PENTILE) {

#if defined (CONFIG_BACKLIGHT_LP8556)
		if (get_bl_type() == BLIC_LP8558_UNMASK)
			lp8556_i2c_info[0].platform_data = &lp8558_unmask_pentile;
#endif

#if defined (CONFIG_BACKLIGHT_PWM)
		period = vienna_bl_data.pwm_period_ns/100;
		for (i = 0; i < 256; i++) {
			if (i <= brightness_level[j])
				brightness_table[i] = i*period*pwm_level[j]/brightness_level[j];
			else {
				brightness_table[i] = (i*period*(pwm_level[j+1]-pwm_level[j]))/(brightness_level[j+1]-brightness_level[j]);
				diff = (brightness_level[j+1]*period*(pwm_level[j+1]-pwm_level[j]))/(brightness_level[j+1]-brightness_level[j])-(period*pwm_level[j+1]);
				brightness_table[i] = brightness_table[i]-diff;
			}
			if (i == brightness_level[j+1])
				j++;
		}
#if defined (CONFIG_MACH_V1)
		if (system_rev >= 0x03)
			vienna_bl_data.pwm_id = 0;
#endif
		samsung_bl_set(&vienna_bl_gpio_info, &vienna_bl_data);
#endif

#if defined (CONFIG_TCON_S6TNDR3X)
		i2c_register_board_info(TCON_I2C_ID, s6tndr3x_i2c_info, ARRAY_SIZE(s6tndr3x_i2c_info));
		ret = platform_device_register(&platform_s6tndr3x_i2c);
		if (ret)
			printk(KERN_ERR "failed to register s6tndr3x_i2c device: %d\n", ret);
#endif
    /* for real lcd */
	} else if (get_lcd_type() == LCD_TYPE_REAL) {

#ifdef CONFIG_BACKLIGHT_LP8556
		if (get_bl_type() == BLIC_LP8558_UNMASK)
			lp8556_i2c_info[0].platform_data = &lp8558_unmask_real;
#endif

#ifdef CONFIG_S5P_MDNIE_PWM
		mdnie_data.support_pwm = 1;
		for (i = 0; i < 256; i++)
			mdnie_pwm_br[i] = (i << 3) + 0x07;
#endif
	} else {
		BUG();
	}

#if defined (CONFIG_BACKLIGHT_LP8556)
	i2c_register_board_info(BLIC_I2C_ID, lp8556_i2c_info, ARRAY_SIZE(lp8556_i2c_info));
#endif

	s5p_fimd1_set_platdata(&vienna_lcd1_pdata);
	dev_set_name(&s5p_device_fimd1.dev, "exynos5-fb.1");
	clk_add_alias("lcd", "exynos5-fb.1", "fimd", &s5p_device_fimd1.dev);
	s5p_dp_set_platdata(&vienna_dp_data);

	platform_add_devices(vienna_display_devices,
			     ARRAY_SIZE(vienna_display_devices));

#ifdef CONFIG_S5P_DP
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_mpll_bpll", 267 * MHZ);
#endif

#ifdef CONFIG_FB_S5P_MDNIE
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
		"sclk_mdnie", "mout_mpll_bpll", 267 * MHZ);
#endif

#ifdef CONFIG_S5P_MDNIE_PWM
	exynos5_set_clk_rate_mdnie_pwm();
#endif


#if 0
	res = platform_get_resource(&s5p_device_fimd1, IORESOURCE_MEM, 1);
	if (res) {
		res->start = vienna_bootloader_fb_start;
		res->end = res->start + vienna_bootloader_fb_size - 1;
		pr_info("bootloader fb located at %8X-%8X\n", res->start,
				res->end);
	} else {
		pr_err("failed to find bootloader fb resource\n");
	}
#endif
}
