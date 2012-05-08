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

#include <plat/cpu.h>
#include <plat/regs-serial.h>
#include <plat/regs-srom.h>
#include <plat/backlight.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>

#include <video/platform_lcd.h>

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

static struct plat_lcd_data smdk5250_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
	.match_fb	= smdk5250_match_fb,
};

static struct platform_device smdk5250_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk5250_mipi_lcd_data,
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
	{},
};

static struct platform_device *smdk5250_devices[] __initdata = {
	&smdk5250_mipi_lcd, /* for platform_lcd device */
	&exynos_device_md0, /* for media device framework */
	&exynos_device_md1, /* for media device framework */
	&exynos_device_md2, /* for media device framework */
	&samsung_asoc_dma,  /* for audio dma interface device */
};

static void __init exynos5250_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(24000000);
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
	.map_io		= exynos5250_dt_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= exynos5250_dt_machine_init,
	.timer		= &exynos4_timer,
	.dt_compat	= exynos5250_dt_compat,
	.restart        = exynos5_restart,
MACHINE_END
