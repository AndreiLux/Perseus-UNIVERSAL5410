/*
 * SAMSUNG EXYNOS5250 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/serial_core.h>
#include <linux/smsc911x.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pwm_backlight.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/regulator/machine.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <mach/map.h>
#include <mach/ohci.h>
#include <mach/regs-pmu.h>
#include <mach/sysmmu.h>

#include <plat/cpu.h>
#include <plat/dsim.h>
#include <plat/fb.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-fb.h>
#include <plat/regs-serial.h>
#include <plat/regs-srom.h>
#include <plat/backlight.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>

#include <video/platform_lcd.h>

#include "drm/exynos_drm.h"
#include "common.h"

static void __init smsc911x_init(int ncs)
{
	u32 data;

	/* configure nCS1 width to 16 bits */
	data = __raw_readl(S5P_SROM_BW) &
		~(S5P_SROM_BW__CS_MASK << (ncs * 4));
	data |= ((1 << S5P_SROM_BW__DATAWIDTH__SHIFT) |
		(1 << S5P_SROM_BW__WAITENABLE__SHIFT) |
		(1 << S5P_SROM_BW__BYTEENABLE__SHIFT)) << (ncs * 4);
	__raw_writel(data, S5P_SROM_BW);

	/* set timing for nCS1 suitable for ethernet chip */
	__raw_writel((0x1 << S5P_SROM_BCX__PMC__SHIFT) |
		(0x9 << S5P_SROM_BCX__TACP__SHIFT) |
		(0xc << S5P_SROM_BCX__TCAH__SHIFT) |
		(0x1 << S5P_SROM_BCX__TCOH__SHIFT) |
		(0x6 << S5P_SROM_BCX__TACC__SHIFT) |
		(0x1 << S5P_SROM_BCX__TCOS__SHIFT) |
		(0x1 << S5P_SROM_BCX__TACS__SHIFT),
		S5P_SROM_BC0 + (ncs * 4));
}

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

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 * 0 | MIE/MDNIE
	 * 1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
}

static struct s3c_fb_platdata smdk5250_lcd1_pdata __initdata = {
	.win[0]		= &smdk5250_fb_win0,
	.win[1]		= &smdk5250_fb_win1,
	.win[2]		= &smdk5250_fb_win2,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
};

static struct mipi_dsim_config dsim_info = {
	.e_interface		= DSIM_VIDEO,
	.e_pixel_format		= DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush		= false,
	.eot_disable		= false,
	.auto_vertical_cnt	= false,
	.hse			= false,
	.hfp			= false,
	.hbp			= false,
	.hsa			= false,

	.e_no_data_lane		= DSIM_DATA_LANE_4,
	.e_byte_clk		= DSIM_PLL_OUT_DIV8,
	.e_burst_mode		= DSIM_BURST,

	.p			= 3,
	.m			= 115,
	.s			= 1,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time	= 500,

	.esc_clk		= 0.4 * 1000000, /* escape clk : 10MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0x0f,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &tc358764_mipi_lcd_driver,
};

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= 0x4,
	.rgb_timing.right_margin	= 0x4,
	.rgb_timing.upper_margin	= 0x4,
	.rgb_timing.lower_margin	=  0x4,
	.rgb_timing.hsync_len		= 0x4,
	.rgb_timing.vsync_len		= 0x4,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= 1280,
	.lcd_size.height		= 800,
};

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

static struct platform_device exynos_drm_device = {
	.name           = "exynos-drm",
	.dev = {
		.dma_mask = &exynos_drm_device.dev.coherent_dma_mask,
		.coherent_dma_mask = 0xffffffffUL,
	}
};

static void lcd_set_power(struct plat_lcd_data *pd,
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
	/*
	 * Request lcd_bl_en GPIO for smdk5250_bl_notify().
	 * TODO: Fix this so we are not at risk of requesting the GPIO
	 * multiple times, this should be done with device tree, and
	 * likely integrated into the plat-samsung/dev-backlight.c init.
	 */
	gpio_request_one(EXYNOS5_GPX3(0), GPIOF_OUT_INIT_LOW, "GPX3");
}

static int smdk5250_match_fb(struct plat_lcd_data *pd, struct fb_info *info)
{
	/* Don't call .set_power callback while unblanking */
	return 0;
}

static struct plat_lcd_data smdk5250_lcd_data = {
	.set_power	= lcd_set_power,
	.match_fb	= smdk5250_match_fb,
};

static struct platform_device smdk5250_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk5250_lcd_data,
};

static int smdk5250_bl_notify(struct device *unused, int brightness)
{
	/* manage lcd_bl_en signal */
	if (brightness)
		gpio_set_value(EXYNOS5_GPX3(0), 1);
	else
		gpio_set_value(EXYNOS5_GPX3(0), 0);

	return brightness;
}

/* LCD Backlight data */
static struct samsung_bl_gpio_info smdk5250_bl_gpio_info = {
	.no	= EXYNOS5_GPB2(0),
	.func	= S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data smdk5250_bl_data = {
	.pwm_period_ns	= 1000,
	.notify		= smdk5250_bl_notify,
};

struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = 0,
};

struct platform_device exynos_device_md1 = {
	.name = "exynos-mdev",
	.id = 1,
};

struct platform_device exynos_device_md2 = {
	.name = "exynos-mdev",
	.id = 2,
};

static struct regulator_consumer_supply wm8994_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "1-001a");

static struct regulator_consumer_supply wm8994_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "1-001a");

static struct regulator_init_data wm8994_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1",
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_avdd1_supply,
};

static struct regulator_init_data wm8994_ldo2_data = {
	.constraints	= {
	.name			= "DCVDD",
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
	.ldo[0] = { 0, &wm8994_ldo1_data },
	.ldo[1] = { 0, &wm8994_ldo2_data },
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("wm8994", 0x1a),
		.platform_data  = &wm8994_platform_data,
	},
};

struct sysmmu_platform_data platdata_sysmmu_mfc_l = {
	.dbgname = "mfc_l",
	.clockname = "sysmmu",
};

struct sysmmu_platform_data platdata_sysmmu_mfc_r = {
	.dbgname = "mfc_r",
	.clockname = "sysmmu",
};

struct sysmmu_platform_data platdata_sysmmu_gsc = {
	.dbgname = "gsc",
	.clockname = "sysmmu",
};

struct sysmmu_platform_data platdata_sysmmu_g2d = {
	.dbgname = "g2d",
	.clockname = "sysmmu",
};

struct sysmmu_platform_data platdata_sysmmu_fimd = {
	.dbgname = "fimd",
	.clockname = "sysmmu",
};

struct sysmmu_platform_data platdata_sysmmu_tv = {
	.dbgname = "tv",
	.clockname = "sysmmu",
};

/*
 * The following lookup table is used to override device names when devices
 * are registered from device tree. This is temporarily added to enable
 * device tree support addition for the EXYNOS5 architecture.
 *
 * For drivers that require platform data to be provided from the machine
 * file, a platform data pointer can also be supplied along with the
 * devices names. Usually, the platform data elements that cannot be parsed
 * from the device tree by the drivers (example: function pointers) are
 * supplied. But it should be noted that this is a temporary mechanism and
 * at some point, the drivers should be capable of parsing all the platform
 * data from the device tree.
 */
static const struct of_dev_auxdata exynos5250_auxdata_lookup[] __initconst = {
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART0,
				"exynos4210-uart.0", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART1,
				"exynos4210-uart.1", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART2,
				"exynos4210-uart.2", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART3,
				"exynos4210-uart.3", NULL),
	OF_DEV_AUXDATA("samsung,s3c2440-i2c", EXYNOS5_PA_IIC(0),
				"s3c2440-i2c.0", NULL),
	OF_DEV_AUXDATA("samsung,s3c2440-i2c", EXYNOS5_PA_IIC(1),
				"s3c2440-i2c.1", NULL),
	OF_DEV_AUXDATA("synopsis,dw-mshc-exynos5250", 0x12200000,
				"dw_mmc.0", NULL),
	OF_DEV_AUXDATA("synopsis,dw-mshc-exynos5250", 0x12210000,
				"dw_mmc.1", NULL),
	OF_DEV_AUXDATA("synopsis,dw-mshc-exynos5250", 0x12220000,
				"dw_mmc.2", NULL),
	OF_DEV_AUXDATA("synopsis,dw-mshc-exynos5250", 0x12230000,
				"dw_mmc.3", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_PDMA0, "dma-pl330.0", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_PDMA1, "dma-pl330.1", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_MDMA1, "dma-pl330.2", NULL),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x10A60000,
				"s5p-sysmmu.2", &platdata_sysmmu_g2d),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x11210000,
				"s5p-sysmmu.3", &platdata_sysmmu_mfc_l),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x11200000,
				"s5p-sysmmu.4", &platdata_sysmmu_mfc_r),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x14640000,
				"s5p-sysmmu.27", &platdata_sysmmu_fimd),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x14650000,
				"s5p-sysmmu.28", &platdata_sysmmu_tv),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x13E80000,
				"s5p-sysmmu.23", &platdata_sysmmu_gsc),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x13E90000,
				"s5p-sysmmu.24", &platdata_sysmmu_gsc),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x13EA0000,
				"s5p-sysmmu.25", &platdata_sysmmu_gsc),
	OF_DEV_AUXDATA("samsung,s5p-sysmmu", 0x13EB0000,
				"s5p-sysmmu.26", &platdata_sysmmu_gsc),
	OF_DEV_AUXDATA("samsung,exynos5-fb", 0x14400000,
				"exynos5-fb", &smdk5250_lcd1_pdata),
	OF_DEV_AUXDATA("samsung,exynos5-mipi", 0x14500000,
				"s5p-mipi-dsim", &dsim_platform_data),
	{},
};

static struct platform_device *smdk5250_devices[] __initdata = {
	&smdk5250_lcd, /* for platform_lcd device */
	&exynos_device_md0, /* for media device framework */
	&exynos_device_md1, /* for media device framework */
	&exynos_device_md2, /* for media device framework */
	&samsung_asoc_dma,  /* for audio dma interface device */
	&exynos_drm_device,
};

static void __init exynos5250_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(24000000);
}

static void __init exynos5_reserve(void)
{
	/* required to have enough address range to remap the IOMMU
	 * allocated buffers */
	init_consistent_dma_size(SZ_64M);
}

static void s5p_tv_setup(void)
{
	/* direct HPD to HDMI chip */
	gpio_request(EXYNOS5_GPX3(7), "hpd-plug");

	gpio_direction_input(EXYNOS5_GPX3(7));
	s3c_gpio_cfgpin(EXYNOS5_GPX3(7), S3C_GPIO_SFN(0x3));
	s3c_gpio_setpull(EXYNOS5_GPX3(7), S3C_GPIO_PULL_NONE);
}

static void exynos5_i2c_setup(void)
{
	/* Setup the low-speed i2c controller interrupts */
	writel(0x0, EXYNOS5_SYS_I2C_CFG);
}

static void __init exynos5250_dt_machine_init(void)
{
	if (of_machine_is_compatible("samsung,smdk5250"))
		smsc911x_init(1);
	samsung_bl_set(&smdk5250_bl_gpio_info, &smdk5250_bl_data);

	if (gpio_request_one(EXYNOS5_GPX2(6), GPIOF_OUT_INIT_HIGH,
		"HOST_VBUS_CONTROL")) {
		printk(KERN_ERR "failed to request gpio_host_vbus\n");
	} else {
		s3c_gpio_setpull(EXYNOS5_GPX2(6), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5_GPX2(6));
	}

	exynos5_i2c_setup();

	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	of_platform_populate(NULL, of_default_bus_match_table,
				exynos5250_auxdata_lookup, NULL);

	s5p_tv_setup();

	platform_add_devices(smdk5250_devices, ARRAY_SIZE(smdk5250_devices));
}

static char const *exynos5250_dt_compat[] __initdata = {
	"samsung,exynos5250",
	NULL
};

DT_MACHINE_START(EXYNOS5_DT, "SAMSUNG EXYNOS5 (Flattened Device Tree)")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.init_irq	= exynos5_init_irq,
	.reserve	= exynos5_reserve,
	.map_io		= exynos5250_dt_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= exynos5250_dt_machine_init,
	.timer		= &exynos4_timer,
	.dt_compat	= exynos5250_dt_compat,
	.restart        = exynos5_restart,
MACHINE_END
