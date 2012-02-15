/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/cma.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/pwm_backlight.h>

#include <video/platform_lcd.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/regs-serial.h>
#include <plat/gpio-cfg.h>
#include <plat/backlight.h>
#include <plat/devs.h>
#include <plat/regs-fb-v4.h>
#include <plat/iic.h>

#include <mach/map.h>
#include <mach/sysmmu.h>
#include <mach/exynos-ion.h>
#include <mach/dwmci.h>

#include <plat/dsim.h>
#include <plat/mipi_dsi.h>

#include "common.h"


/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK5250_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK5250_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK5250_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg smdk5250_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("s5m87xx", 0xCC >> 1),
		.irq		= IRQ_EINT(26),
	},
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("wm8994", 0x1a),
	},
};

static struct i2c_board_info i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO("exynos_hdcp", (0x74 >> 1)),
	},
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("egalax_i2c", 0x04),
		.irq		= IRQ_EINT(25),
	},
};

#ifdef CONFIG_CMA
/* defined in arch/arm/mach-exynos/reserve-mem.c */
extern void exynos_cma_region_reserve(struct cma_region *,
				struct cma_region *, size_t, const char *);

static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
			{
				.alignment = SZ_1M
			}
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD
		}, {
			.name = "fimd",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD * SZ_1K,
			.start = 0
#endif
		}, {
			.size = 0 /* END OF REGION DEFINITIONS */
		}
	};

	static const char map[] __initconst =
		"exynos5-fb.1=fimd;"
		"ion-exynos=ion, fimd;";

	exynos_cma_region_reserve(regions, NULL, 0, map);
}
#else /* CONFIG_CMA */
static inline void exynos_reserve_mem(void)
{
}
#endif

static void exynos_dwmci_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS5_GPC0(0); gpio < EXYNOS5_GPC0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS5_GPC1(3); gpio <= EXYNOS5_GPC1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS5_GPC0(3); gpio <= EXYNOS5_GPC0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS5_GPC0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}
}

static struct dw_mci_board exynos_dwmci_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 66 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	.fifo_depth             = 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci_cfg_gpio,
};

#ifdef CONFIG_FB_S3C
#if defined(CONFIG_LCD_MIPI_S6E8AB0)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* reset */
	gpio_request_one(EXYNOS5_GPX1(5), GPIOF_OUT_INIT_HIGH, "GPX1");

	mdelay(20);
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX1(5), 0);
		mdelay(20);
		gpio_set_value(EXYNOS5_GPX1(5), 1);
		mdelay(20);
		gpio_free(EXYNOS5_GPX1(5));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX1(5), 0);
		mdelay(20);
		gpio_set_value(EXYNOS5_GPX1(5), 1);
		mdelay(20);
		gpio_free(EXYNOS5_GPX1(5));
	}
	mdelay(20);
	/* power */
	gpio_request_one(EXYNOS5_GPX3(0), GPIOF_OUT_INIT_LOW, "GPX3");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX3(0), 1);
		gpio_free(EXYNOS5_GPX3(0));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX3(0), 0);
		gpio_free(EXYNOS5_GPX3(0));
	}

#ifndef CONFIG_BACKLIGHT_PWM
	/* backlight */
	gpio_request_one(EXYNOS5_GPB2(0), GPIOF_OUT_INIT_LOW, "GPB2");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPB2(0), 1);
		gpio_free(EXYNOS5_GPB2(0));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPB2(0), 0);
		gpio_free(EXYNOS5_GPB2(0));
	}
#endif
}

static struct plat_lcd_data smdk5250_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device smdk5250_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk5250_mipi_lcd_data,
};

static struct s3c_fb_pd_win smdk5250_fb_win0 = {
	.win_mode = {
		.left_margin	= 0x4,
		.right_margin	= 0x4,
		.upper_margin	= 4,
		.lower_margin	= 4,
		.hsync_len	= 4,
		.vsync_len	= 4,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win smdk5250_fb_win1 = {
	.win_mode = {
		.left_margin	= 0x4,
		.right_margin	= 0x4,
		.upper_margin	= 4,
		.lower_margin	= 4,
		.hsync_len	= 4,
		.vsync_len	= 4,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win smdk5250_fb_win2 = {
	.win_mode = {
		.left_margin	= 0x4,
		.right_margin	= 0x4,
		.upper_margin	= 4,
		.lower_margin	= 4,
		.hsync_len	= 4,
		.vsync_len	= 4,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#elif defined(CONFIG_LCD_MIPI_TC358764)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* reset */
	gpio_request_one(EXYNOS5_GPX1(5), GPIOF_OUT_INIT_HIGH, "GPX1");

	mdelay(20);
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX1(5), 0);
		mdelay(20);
		gpio_set_value(EXYNOS5_GPX1(5), 1);
		mdelay(20);
		gpio_free(EXYNOS5_GPX1(5));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX1(5), 0);
		mdelay(20);
		gpio_set_value(EXYNOS5_GPX1(5), 1);
		mdelay(20);
		gpio_free(EXYNOS5_GPX1(5));
	}
	mdelay(20);
	/* power */
	gpio_request_one(EXYNOS5_GPX3(0), GPIOF_OUT_INIT_LOW, "GPX3");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX3(0), 1);
		gpio_free(EXYNOS5_GPX3(0));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX3(0), 0);
		gpio_free(EXYNOS5_GPX3(0));
	}

#ifndef CONFIG_BACKLIGHT_PWM
	/* backlight */
	gpio_request_one(EXYNOS5_GPB2(0), GPIOF_OUT_INIT_LOW, "GPB2");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPB2(0), 1);
		gpio_free(EXYNOS5_GPB2(0));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPB2(0), 0);
		gpio_free(EXYNOS5_GPB2(0));
	}
#endif
}

static struct plat_lcd_data smdk5250_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device smdk5250_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk5250_mipi_lcd_data,
};

static struct s3c_fb_pd_win smdk5250_fb_win0 = {
	.win_mode = {
		.left_margin	= 4,
		.right_margin	= 4,
		.upper_margin	= 4,
		.lower_margin	= 4,
		.hsync_len	= 4,
		.vsync_len	= 4,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win smdk5250_fb_win1 = {
	.win_mode = {
		.left_margin	= 4,
		.right_margin	= 4,
		.upper_margin	= 4,
		.lower_margin	= 4,
		.hsync_len	= 4,
		.vsync_len	= 4,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
static struct s3c_fb_pd_win smdk5250_fb_win2 = {
	.win_mode = {
		.left_margin	= 4,
		.right_margin	= 4,
		.upper_margin	= 4,
		.lower_margin	= 4,
		.hsync_len	= 4,
		.vsync_len	= 4,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif
static struct s3c_fb_platdata smdk5250_lcd1_pdata __initdata = {
#if defined(CONFIG_LCD_MIPI_S6E8AB0)
	.win[0]		= &smdk5250_fb_win0,
	.win[1]		= &smdk5250_fb_win1,
	.win[2]		= &smdk5250_fb_win2,
#elif defined(CONFIG_LCD_MIPI_TC358764)
	.win[0]		= &smdk5250_fb_win0,
	.win[1]		= &smdk5250_fb_win1,
	.win[2]		= &smdk5250_fb_win2,
#endif
	.default_win	= 2,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_LCD_MIPI_S6E8AB0)
	.vidcon1	= VIDCON1_INV_VCLK,
#elif defined(CONFIG_LCD_MIPI_TC358764)
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= exynos5_fimd1_gpio_setup_24bpp,
};

#endif

#ifdef CONFIG_FB_MIPI_DSIM
#if defined(CONFIG_LCD_MIPI_S6E8AB0)
static struct mipi_dsim_config dsim_info = {
	.e_interface     = DSIM_VIDEO,
	.e_pixel_format  = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush		= false,
	.eot_disable		= false,
	.auto_vertical_cnt	= true,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane	= DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 2,
	.m = 57,
	.s = 1,
	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 20 * 1000000,	/* escape clk : 10MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e8ab0_mipi_lcd_driver,
};
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin  = 0xa,
	.rgb_timing.right_margin = 0xa,
	.rgb_timing.upper_margin = 80,
	.rgb_timing.lower_margin = 48,
	.rgb_timing.hsync_len	 = 5,
	.rgb_timing.vsync_len	 = 32,
	.cpu_timing.cs_setup	 = 0,
	.cpu_timing.wr_setup	 = 1,
	.cpu_timing.wr_act	 = 0,
	.cpu_timing.wr_hold	 = 0,
	.lcd_size.width		 = 1280,
	.lcd_size.height	 = 800,
};
#elif defined (CONFIG_LCD_MIPI_TC358764)
static struct mipi_dsim_config dsim_info = {
	.e_interface	 = DSIM_VIDEO,
	.e_pixel_format  = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	 = false,
	.eot_disable	 = false,
	.auto_vertical_cnt = false,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane  = DSIM_DATA_LANE_4,
	.e_byte_clk	 = DSIM_PLL_OUT_DIV8,
	.e_burst_mode	 = DSIM_BURST,

	.p = 3,
	.m = 115,
	.s = 1,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 0.4 * 1000000,	/* escape clk : 10MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0x0f,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &tc358764_mipi_lcd_driver,
};

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin  = 0x4,
	.rgb_timing.right_margin = 0x4,
	.rgb_timing.upper_margin = 0x4,
	.rgb_timing.lower_margin = 0x4,
	.rgb_timing.hsync_len	 = 0x4,
	.rgb_timing.vsync_len	 = 0x4,
	.cpu_timing.cs_setup	 = 0,
	.cpu_timing.wr_setup	 = 1,
	.cpu_timing.wr_act	 = 0,
	.cpu_timing.wr_hold	 = 0,
	.lcd_size.width		 = 1280,
	.lcd_size.height	 = 800,
};
#endif

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim0",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.part_reset		= s5p_dsim_part_reset,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,
	/*
	* the stable time of needing to write data on SFR
	* when the mipi mode becomes LP mode.
	*/
	.delay_for_stabilization = 600,
};
#endif

/* LCD Backlight data */
static struct samsung_bl_gpio_info smdk5250_bl_gpio_info = {
	.no	= EXYNOS5_GPB2(0),
	.func	= S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data smdk5250_bl_data = {
	.pwm_id		= 0,
	.pwm_period_ns	= 30000,
};

static struct platform_device *smdk5250_devices[] __initdata = {
	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c2,
	&s3c_device_i2c4,
	&s3c_device_i2c5,
	&s3c_device_i2c7,
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
	&SYSMMU_PLATDEV(2d),
	&SYSMMU_PLATDEV(jpeg),
	&SYSMMU_PLATDEV(gsc0),
	&SYSMMU_PLATDEV(gsc1),
	&SYSMMU_PLATDEV(gsc2),
	&SYSMMU_PLATDEV(gsc3),
	&SYSMMU_PLATDEV(flite0),
	&SYSMMU_PLATDEV(flite1),
	&SYSMMU_PLATDEV(tv),
	&SYSMMU_PLATDEV(rot),
	&SYSMMU_PLATDEV(is_isp),
	&SYSMMU_PLATDEV(is_drc),
	&SYSMMU_PLATDEV(is_fd),
	&SYSMMU_PLATDEV(is_cpu),
	&SYSMMU_PLATDEV(is_odc),
	&SYSMMU_PLATDEV(is_sclrc),
	&SYSMMU_PLATDEV(is_sclrp),
	&SYSMMU_PLATDEV(is_dis0),
	&SYSMMU_PLATDEV(is_dis1),
	&SYSMMU_PLATDEV(is_3dnr),
#ifdef CONFIG_ION_EXYNOS
	&exynos_device_ion,
#endif
	&exynos5_device_dwmci,
#ifdef CONFIG_FB_S3C
#ifdef CONFIG_FB_MIPI_DSIM
	&smdk5250_mipi_lcd,
	&s5p_device_mipi_dsim,
#endif
	&s5p_device_fimd1,
#endif
};

static void __init smdk5250_map_io(void)
{
	clk_xusbxti.rate = 24000000;

	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(smdk5250_uartcfgs, ARRAY_SIZE(smdk5250_uartcfgs));
}

static void __init exynos_sysmmu_init(void)
{
#ifdef CONFIG_VIDEO_JPEG_V2X
	sysmmu_set_owner(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_l).dev, &s5p_device_mfc.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_r).dev, &s5p_device_mfc.dev);
#endif
#if defined(CONFIG_VIDEO_EXYNOS_TV)
	ASSIGN_SYSMMU_POWERDOMAIN(tv, &exynos5_device_pd[PD_DISP1].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_mixer.dev);

#endif
#ifdef CONFIG_VIDEO_EXYNOS_GSCALER
	ASSIGN_SYSMMU_POWERDOMAIN(gsc0, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(gsc1, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(gsc2, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(gsc3, &exynos5_device_pd[PD_GSCL].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc0).dev, &exynos5_device_gsc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc1).dev, &exynos5_device_gsc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc2).dev, &exynos5_device_gsc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc3).dev, &exynos5_device_gsc3.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	ASSIGN_SYSMMU_POWERDOMAIN(flite0, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(flite1, &exynos5_device_pd[PD_GSCL].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(flite0).dev,
						&exynos_device_flite0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(flite1).dev,
						&exynos_device_flite1.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_ROTATOR
	sysmmu_set_owner(&SYSMMU_PLATDEV(rot).dev, &exynos_device_rotator.dev);
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(2d).dev, &s5p_device_fimg2d.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS5_FIMC_IS
	ASSIGN_SYSMMU_POWERDOMAIN(is_isp, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_drc, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_fd, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_cpu, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_odc, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_sclrc, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_sclrp, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_dis0, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_dis1, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_3dnr, &exynos5_device_pd[PD_ISP].dev);

	sysmmu_set_owner(&SYSMMU_PLATDEV(is_isp).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_drc).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_fd).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_cpu).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_odc).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_sclrc).dev
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_sclrp).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_dis0).dev
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_dis1).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_3dnr).dev,
						&exynos5_device_fimc_is.dev);
#endif
}

static void __init smdk5250_machine_init(void)
{
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));

	s3c_i2c4_set_platdata(NULL);
	s3c_i2c5_set_platdata(NULL);
	s3c_i2c7_set_platdata(NULL);
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

	exynos_sysmmu_init();
	exynos_reserve_mem();
	exynos_ion_set_platdata();
	exynos_dwmci_set_platdata(&exynos_dwmci_pdata);

	samsung_bl_set(&smdk5250_bl_gpio_info, &smdk5250_bl_data);

#ifdef CONFIG_FB_S3C
	s5p_fimd1_set_platdata(&smdk5250_lcd1_pdata);
	dev_set_name(&s5p_device_fimd1.dev, "exynos5-fb.1");
	clk_add_alias("lcd", "exynos5-fb.1", "fimd", &s5p_device_fimd1.dev);
#endif

#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim_set_platdata(&dsim_platform_data);
#endif

	platform_add_devices(smdk5250_devices, ARRAY_SIZE(smdk5250_devices));

#ifdef CONFIG_FB_S3C
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev, "sclk_fimd", "mout_mpll_user",
				800 * MHZ);
#endif
}

MACHINE_START(SMDK5250, "SMDK5250")
	.atag_offset	= 0x100,
	.init_irq	= exynos5_init_irq,
	.map_io		= smdk5250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= smdk5250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos5_restart,
MACHINE_END
