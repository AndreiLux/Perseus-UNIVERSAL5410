/* linux/arch/arm/mach-exynos/mach-smdk4212.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/input.h>
#include <linux/mmc/host.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max8649.h>
#include <linux/regulator/fixed.h>
#ifdef CONFIG_LEDS_AAT1290A
#include <linux/leds-aat1290a.h>
#endif
#ifdef CONFIG_MFD_MAX77693
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/leds-max77693.h>
#endif

#ifdef CONFIG_BATTERY_MAX17047_FUELGAUGE
#include <linux/battery/max17047_fuelgauge.h>
#endif
#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/power_supply.h>
#include <linux/battery/samsung_battery.h>
#endif
#include <linux/v4l2-mediabus.h>
#include <linux/memblock.h>
#include <linux/delay.h>
#include <linux/bootmem.h>

#ifdef CONFIG_DMA_CMA
#include <linux/dma-contiguous.h>
#endif

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/exynos4.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/keypad.h>
#include <plat/devs.h>
#include <plat/fb-s5p.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/backlight.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/pd.h>
#include <plat/sdhci.h>
#include <plat/mshci.h>
#include <plat/ehci.h>
#include <plat/usbgadget.h>
#include <plat/s3c64xx-spi.h>
#include <plat/tvout.h>
#include <plat/csis.h>
#include <plat/media.h>
#include <plat/adc.h>
#include <media/exynos_fimc_is.h>
#include <mach/exynos-ion.h>

#include <mach/map.h>
#include <mach/spi-clocks.h>

#include <mach/dev.h>
#include <mach/ppmu.h>

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
#include <plat/s5p-tmu.h>
#include <mach/regs-tmu.h>
#endif

#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
#include <plat/s5p-mfc.h>
#endif

#include <plat/fb-s5p.h>

#ifdef CONFIG_FB_S5P_EXTDSP
struct s3cfb_extdsp_lcd {
	int	width;
	int	height;
	int	bpp;
};
#endif
#include <mach/dev-sysmmu.h>

#ifdef CONFIG_VIDEO_JPEG_V2X
#include <plat/jpeg.h>
#endif

#include <plat/fimg2d.h>
#include <plat/s5p-sysmmu.h>

#include <mach/sec_debug.h>

#include <mach/gpio-midas.h>

#include <mach/midas-power.h>

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#endif
#include <mach/midas-thermistor.h>
#include <mach/midas-tsp.h>
#include <mach/regs-clock.h>

#include <mach/midas-lcd.h>
#include <mach/midas-sound.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
#include <linux/i2c/touchkey_i2c.h>
#endif

#include "board-mobile.h"

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK4212_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK4212_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK4212_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

#define SMDK4212_UFCON_GPS	(S3C2410_UFCON_FIFOMODE |	\
				S5PV210_UFCON_TXTRIG8 |	\
				S5PV210_UFCON_RXTRIG32)

static struct s3c2410_uartcfg smdk4212_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK4212_UCON_DEFAULT,
		.ulcon		= SMDK4212_ULCON_DEFAULT,
		.ufcon		= SMDK4212_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK4212_UCON_DEFAULT,
		.ulcon		= SMDK4212_ULCON_DEFAULT,
		.ufcon		= SMDK4212_UFCON_GPS,
		.set_runstate	= set_gps_uart_op,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK4212_UCON_DEFAULT,
		.ulcon		= SMDK4212_ULCON_DEFAULT,
		.ufcon		= SMDK4212_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK4212_UCON_DEFAULT,
		.ulcon		= SMDK4212_ULCON_DEFAULT,
		.ufcon		= SMDK4212_UFCON_DEFAULT,
	},
};

#if defined(CONFIG_S3C64XX_DEV_SPI)
#if defined(CONFIG_VIDEO_S5C73M3_SPI)

static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x00,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias = "s5c73m3_spi",
		.platform_data = NULL,
		.max_speed_hz = 50000000,
		.bus_num = 1,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi1_csi[0],
	}
};
#endif
#endif

static struct i2c_board_info i2c_devs8_emul[];

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
static void touchkey_init_hw(void)
{
#if defined(CONFIG_MACH_M0) || defined(CONFIG_MACH_C1)
	if (system_rev < 11)
		return; /* rev 1.0 */
#endif

#if defined(CONFIG_TARGET_LOCALE_KOR) || \
	defined(CONFIG_MACH_M0) || \
	defined(CONFIG_MACH_C1) || \
	defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_BAFFIN)
	gpio_request(GPIO_3_TOUCH_EN, "gpio_3_touch_en");
#endif

	gpio_request(GPIO_3_TOUCH_INT, "3_TOUCH_INT");
	s3c_gpio_setpull(GPIO_3_TOUCH_INT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_3_TOUCH_INT);
	gpio_direction_input(GPIO_3_TOUCH_INT);

	i2c_devs8_emul[0].irq = gpio_to_irq(GPIO_3_TOUCH_INT);
	irq_set_irq_type(gpio_to_irq(GPIO_3_TOUCH_INT), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(GPIO_3_TOUCH_INT, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(GPIO_3_TOUCH_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_3_TOUCH_SDA, S3C_GPIO_PULL_DOWN);
}

static int touchkey_suspend(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator))
		return 0;
	if (regulator_is_enabled(regulator))
		regulator_force_disable(regulator);

	s3c_gpio_setpull(GPIO_3_TOUCH_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_3_TOUCH_SDA, S3C_GPIO_PULL_DOWN);

	regulator_put(regulator);

	return 1;
}

static int touchkey_resume(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator))
		return 0;
	regulator_enable(regulator);
	regulator_put(regulator);

	s3c_gpio_setpull(GPIO_3_TOUCH_SCL, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_3_TOUCH_SDA, S3C_GPIO_PULL_NONE);

	return 1;
}

static int touchkey_power_on(bool on)
{
	int ret;

	if (on) {
		gpio_direction_output(GPIO_3_TOUCH_INT, 1);
		irq_set_irq_type(gpio_to_irq(GPIO_3_TOUCH_INT),
			IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(GPIO_3_TOUCH_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_3_TOUCH_INT, S3C_GPIO_PULL_NONE);
	} else
		gpio_direction_input(GPIO_3_TOUCH_INT);

	if (on)
		ret = touchkey_resume();
	else
		ret = touchkey_suspend();

	return ret;
}

static int touchkey_led_power_on(bool on)
{
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, "touch_led");
		if (IS_ERR(regulator))
			return 0;
		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "touch_led");
		if (IS_ERR(regulator))
			return 0;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 1;
}

static struct touchkey_platform_data touchkey_pdata = {
	.gpio_sda = GPIO_3_TOUCH_SDA,
	.gpio_scl = GPIO_3_TOUCH_SCL,
	.gpio_int = GPIO_3_TOUCH_INT,
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
};
#endif /*CONFIG_KEYBOARD_CYPRESS_TOUCH*/

#ifdef CONFIG_LEDS_AAT1290A
static int aat1290a_initGpio(void)
{
	int err;

	err = gpio_request(GPIO_CAM_SW_EN, "CAM_SW_EN");
	if (err) {
		printk(KERN_ERR "failed to request CAM_SW_EN\n");
		return -EPERM;
	}
	gpio_direction_output(GPIO_CAM_SW_EN, 1);

	return 0;
}

static void aat1290a_switch(int enable)
{
	gpio_set_value(GPIO_CAM_SW_EN, enable);
}

static int aat1290a_setGpio(void)
{
	int err;

	err = gpio_request(GPIO_TORCH_EN, "TORCH_EN");
	if (err) {
		printk(KERN_ERR "failed to request TORCH_EN\n");
		return -EPERM;
	}
	gpio_direction_output(GPIO_TORCH_EN, 0);
	err = gpio_request(GPIO_TORCH_SET, "TORCH_SET");
	if (err) {
		printk(KERN_ERR "failed to request TORCH_SET\n");
		gpio_free(GPIO_TORCH_EN);
		return -EPERM;
	}
	gpio_direction_output(GPIO_TORCH_SET, 0);

	return 0;
}

static int aat1290a_freeGpio(void)
{
	gpio_free(GPIO_TORCH_EN);
	gpio_free(GPIO_TORCH_SET);

	return 0;
}

static void aat1290a_torch_en(int onoff)
{
	gpio_set_value(GPIO_TORCH_EN, onoff);
}

static void aat1290a_torch_set(int onoff)
{
	gpio_set_value(GPIO_TORCH_SET, onoff);
}

static struct aat1290a_led_platform_data aat1290a_led_data = {
	.brightness = TORCH_BRIGHTNESS_50,
	.status	= STATUS_UNAVAILABLE,
	.switch_sel = aat1290a_switch,
	.initGpio = aat1290a_initGpio,
	.setGpio = aat1290a_setGpio,
	.freeGpio = aat1290a_freeGpio,
	.torch_en = aat1290a_torch_en,
	.torch_set = aat1290a_torch_set,
};

static struct platform_device s3c_device_aat1290a_led = {
	.name	= "aat1290a-led",
	.id	= -1,
	.dev	= {
		.platform_data	= &aat1290a_led_data,
	},
};
#endif

static DEFINE_MUTEX(notify_lock);

#define DEFINE_MMC_CARD_NOTIFIER(num) \
static void (*hsmmc##num##_notify_func)(struct platform_device *, int state); \
static int ext_cd_init_hsmmc##num(void (*notify_func)( \
			struct platform_device *, int state)) \
{ \
	mutex_lock(&notify_lock); \
	WARN_ON(hsmmc##num##_notify_func); \
	hsmmc##num##_notify_func = notify_func; \
	mutex_unlock(&notify_lock); \
	return 0; \
} \
static int ext_cd_cleanup_hsmmc##num(void (*notify_func)( \
			struct platform_device *, int state)) \
{ \
	mutex_lock(&notify_lock); \
	WARN_ON(hsmmc##num##_notify_func != notify_func); \
	hsmmc##num##_notify_func = NULL; \
	mutex_unlock(&notify_lock); \
	return 0; \
}

#ifdef CONFIG_S3C_DEV_HSMMC3
	DEFINE_MMC_CARD_NOTIFIER(3)
#endif

/*
 * call this when you need sd stack to recognize insertion or removal of card
 * that can't be told by SDHCI regs
 */
void mmc_force_presence_change(struct platform_device *pdev)
{
	void (*notify_func)(struct platform_device *, int state) = NULL;
	mutex_lock(&notify_lock);
#ifdef CONFIG_S3C_DEV_HSMMC3
	if (pdev == &s3c_device_hsmmc3)
		notify_func = hsmmc3_notify_func;
#endif

	if (notify_func)
		notify_func(pdev, 1);
	else
		pr_warn("%s: called for device with no notifier\n", __func__);
	mutex_unlock(&notify_lock);
}
EXPORT_SYMBOL_GPL(mmc_force_presence_change);

#ifdef CONFIG_S3C_DEV_HSMMC2
static struct s3c_sdhci_platdata smdk4212_hsmmc2_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_GPIO,
	.ext_cd_gpio		= EXYNOS4_GPX3(4),
	.ext_cd_gpio_invert	= true,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
	.vmmc_name		= "vtf_2.8v"
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
static struct s3c_sdhci_platdata smdk4212_hsmmc3_pdata __initdata = {
/* new code for brm4334 */
	.cd_type		= S3C_SDHCI_CD_EXTERNAL,

	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
	.pm_flags = S3C_SDHCI_PM_IGNORE_SUSPEND_RESUME,
	.ext_cd_init = ext_cd_init_hsmmc3,
	.ext_cd_cleanup = ext_cd_cleanup_hsmmc3,
};
#endif

#ifdef CONFIG_EXYNOS4_DEV_MSHC
static struct s3c_mshci_platdata exynos4_mshc_pdata __initdata = {
	.cd_type		= S3C_MSHCI_CD_PERMANENT,
	.fifo_depth		= 0x80,
#if defined(CONFIG_EXYNOS4_MSHC_8BIT) && \
	defined(CONFIG_EXYNOS4_MSHC_DDR)
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_CMD23,
	.host_caps2		= MMC_CAP2_ADAPT_PACKED | MMC_CAP2_PACKED_CMD,
	.int_power_gpio		= GPIO_eMMC_EN,
};
#endif

#ifdef CONFIG_USB_EHCI_S5P
static struct s5p_ehci_platdata smdk4212_ehci_pdata;

static void __init smdk4212_ehci_init(void)
{
	struct s5p_ehci_platdata *pdata = &smdk4212_ehci_pdata;

	s5p_ehci_set_platdata(pdata);
}
#endif

#ifdef CONFIG_USB_OHCI_S5P
static struct s5p_ohci_platdata smdk4212_ohci_pdata;

static void __init smdk4212_ohci_init(void)
{
	struct s5p_ohci_platdata *pdata = &smdk4212_ohci_pdata;

	s5p_ohci_set_platdata(pdata);
}
#endif

/* USB GADGET */
#ifdef CONFIG_USB_GADGET
static struct s5p_usbgadget_platdata smdk4212_usbgadget_pdata;

#include <linux/usb/android_composite.h>
static void __init smdk4212_usbgadget_init(void)
{
	struct s5p_usbgadget_platdata *pdata = &smdk4212_usbgadget_pdata;
	struct android_usb_platform_data *android_pdata =
		s3c_device_android_usb.dev.platform_data;
	if (android_pdata) {
		unsigned int cdfs = 0;
		unsigned int newluns = 2;

		printk(KERN_DEBUG "usb: %s: default luns=%d, new luns=%d\n",
				__func__, android_pdata->nluns, newluns);
		android_pdata->nluns = newluns;
		android_pdata->cdfs_support = cdfs;
	} else {
		printk(KERN_DEBUG "usb: %s android_pdata is not available\n",
				__func__);
	}

	s5p_usbgadget_set_platdata(pdata);
}
#endif

#ifdef CONFIG_MFD_MAX77693
#ifdef CONFIG_VIBETONZ
static struct max77693_haptic_platform_data max77693_haptic_pdata = {
	.reg2 = MOTOR_LRA | EXT_PWM | DIVIDER_128,
	.pwm_id = 0,
	.init_hw = NULL,
	.motor_en = NULL,
#endif
	.max_timeout = 10000,
	.duty = 35500,
	.period = 37904,
	.regulator_name = "vmotor",
};
#endif

#ifdef CONFIG_BATTERY_MAX77693_CHARGER
static struct max77693_charger_platform_data max77693_charger_pdata = {
#ifdef CONFIG_BATTERY_WPC_CHARGER
	.wpc_irq_gpio = GPIO_WPC_INT,
#if defined(CONFIG_MACH_M0) || defined(CONFIG_MACH_C1) || \
	defined(CONFIG_MACH_M3) || defined(CONFIG_MACH_T0)
	.vbus_irq_gpio = GPIO_V_BUS_INT,
#endif
	.wc_pwr_det = false,
#endif
};
#endif

extern struct max77693_muic_data max77693_muic;
extern struct max77693_regulator_data max77693_regulators;

static bool is_muic_default_uart_path_cp(void)
{
#ifdef CONFIG_MACH_M0
	if (system_rev == 5)
		return true;
#endif
	return false;
}

struct max77693_platform_data exynos4_max77693_info = {
	.irq_base	= IRQ_BOARD_IFIC_START,
	.irq_gpio	= GPIO_IF_PMIC_IRQ,
	.wakeup		= 1,
	.muic = &max77693_muic,
	.is_default_uart_path_cp =  is_muic_default_uart_path_cp,
	.regulators = &max77693_regulators,
	.num_regulators = MAX77693_REG_MAX,
#ifdef CONFIG_VIBETONZ
	.haptic_data = &max77693_haptic_pdata,
#endif
#ifdef CONFIG_BATTERY_MAX77693_CHARGER
	.charger_data = &max77693_charger_pdata,
#endif
};
#endif

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
};

#ifdef CONFIG_S3C_DEV_I2C4
#ifdef CONFIG_MFD_MAX77693
static struct i2c_board_info i2c_devs4_max77693[] __initdata = {
	{
		I2C_BOARD_INFO("max77693", (0xCC >> 1)),
		.platform_data	= &exynos4_max77693_info,
	}
};
#endif
#endif

#ifdef CONFIG_S3C_DEV_I2C5
static struct i2c_board_info i2c_devs5[] __initdata = {
#ifdef CONFIG_REGULATOR_MAX8997
	{
		I2C_BOARD_INFO("max8997", (0xcc >> 1)),
		.platform_data = &exynos4_max8997_info,
	},
#endif

#if defined(CONFIG_REGULATOR_MAX77686)
	/* max77686 on i2c5 other than M1 board */
	{
		I2C_BOARD_INFO("max77686", (0x12 >> 1)),
		.platform_data	= &exynos4_max77686_info,
	},
#endif
};

struct s3c2410_platform_i2c default_i2c5_data __initdata = {
	.bus_num        = 5,
	.flags          = 0,
	.slave_addr     = 0x10,
	.frequency      = 100*1000,
	.sda_delay      = 100,
};

#endif

#ifdef CONFIG_S3C_DEV_I2C6
static struct i2c_board_info i2c_devs6[] __initdata = {
};
#endif /* CONFIG_S3C_DEV_I2C6 */

static struct i2c_board_info i2c_devs7[] __initdata = {
#if defined(CONFIG_MACH_C1) || \
	defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_M0) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_BAFFIN)
#if defined(CONFIG_REGULATOR_MAX77686) /* max77686 on i2c7 with M1 board */
	{
		I2C_BOARD_INFO("max77686", (0x12 >> 1)),
		.platform_data	= &exynos4_max77686_info,
	},
#endif
#endif
};

/* Bluetooth */
#ifdef CONFIG_BT_BCM4334
static struct platform_device bcm4334_bluetooth_device = {
	.name = "bcm4334_bluetooth",
	.id = -1,
};
#endif

static struct i2c_gpio_platform_data gpio_i2c_data8 = {
	.sda_pin = GPIO_3_TOUCH_SDA,
	.scl_pin = GPIO_3_TOUCH_SCL,
};

struct platform_device s3c_device_i2c8 = {
	.name = "i2c-gpio",
	.id = 8,
	.dev.platform_data = &gpio_i2c_data8,
};

/* I2C8 */
static struct i2c_board_info i2c_devs8_emul[] = {
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	{
		I2C_BOARD_INFO("sec_touchkey", 0x20),
		.platform_data = &touchkey_pdata,
	},
#endif
};

/* I2C9 */
static struct i2c_board_info i2c_devs9_emul[] __initdata = {
};

/* I2C10 */
static struct i2c_board_info i2c_devs10_emul[] __initdata = {
};

/* I2C11 */
static struct i2c_board_info i2c_devs11_emul[] __initdata = {
};

/* I2C12 */
#if defined(CONFIG_PN65N_NFC) && \
	!defined(CONFIG_MACH_C1) && !defined(CONFIG_MACH_BAFFIN)
static struct i2c_board_info i2c_devs12_emul[] __initdata = {
};
#endif

#ifdef CONFIG_BATTERY_MAX17047_FUELGAUGE
static struct i2c_gpio_platform_data gpio_i2c_data14 = {
	.sda_pin = GPIO_FUEL_SDA,
	.scl_pin = GPIO_FUEL_SCL,
};

struct platform_device s3c_device_i2c14 = {
	.name = "i2c-gpio",
	.id = 14,
	.dev.platform_data = &gpio_i2c_data14,
};

static struct max17047_platform_data max17047_pdata = {
	.irq_gpio = GPIO_FUEL_ALERT,
};

/* I2C14 */
static struct i2c_board_info i2c_devs14_emul[] __initdata = {
	{
		I2C_BOARD_INFO("max17047-fuelgauge", 0x36),
		.platform_data = &max17047_pdata,
	},
};
#endif

#ifdef CONFIG_SAMSUNG_MHL
/* I2C15 */
static struct i2c_gpio_platform_data gpio_i2c_data15 = {
	.sda_pin = GPIO_MHL_SDA_1_8V,
	.scl_pin = GPIO_MHL_SCL_1_8V,
	.udelay = 3,
	.timeout = 0,
};

struct platform_device s3c_device_i2c15 = {
	.name = "i2c-gpio",
	.id = 15,
	.dev = {
		.platform_data = &gpio_i2c_data15,
	}
};

static struct i2c_board_info i2c_devs15_emul[] __initdata = {
};

/* I2C16 */
#if !defined(CONFIG_MACH_T0) && !defined(CONFIG_MACH_M3)
static struct i2c_gpio_platform_data gpio_i2c_data16 = {
	.sda_pin = GPIO_MHL_DSDA_2_8V,
	.scl_pin = GPIO_MHL_DSCL_2_8V,
};

struct platform_device s3c_device_i2c16 = {
	.name = "i2c-gpio",
	.id = 16,
	.dev.platform_data = &gpio_i2c_data16,
};
#endif /* !defined(CONFIG_MACH_T0) */

static struct i2c_board_info i2c_devs16_emul[] __initdata = {
};
#endif

#if defined(CONFIG_MACH_C1) || \
	defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_M0) || \
	defined(CONFIG_MACH_GC1) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_BAFFIN)
static struct i2c_gpio_platform_data gpio_i2c_data17 = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
};

struct platform_device s3c_device_i2c17 = {
	.name = "i2c-gpio",
	.id = 17,
	.dev.platform_data = &gpio_i2c_data17,
};

/* I2C17 */
static struct i2c_board_info i2c_devs17_emul[] __initdata = {
#ifdef CONFIG_MFD_MAX77693
	{
		I2C_BOARD_INFO("max77693", (0xCC >> 1)),
		.platform_data	= &exynos4_max77693_info,
	}
#endif
};
#endif

/* I2C21 */
#if defined(CONFIG_LEDS_AN30259A) || defined(CONFIG_LEDS_LP5521)
static struct i2c_gpio_platform_data gpio_i2c_data21 = {
	.scl_pin = GPIO_S_LED_I2C_SCL,
	.sda_pin = GPIO_S_LED_I2C_SDA,
};

struct platform_device s3c_device_i2c21 = {
	.name = "i2c-gpio",
	.id = 21,
	.dev.platform_data = &gpio_i2c_data21,
};
#endif

/* I2C21 */
#ifdef CONFIG_LEDS_AN30259A
static struct i2c_board_info i2c_devs21_emul[] __initdata = {
	{
		I2C_BOARD_INFO("an30259a", 0x30),
	},
};
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resource[] = {
	{
		.flags = IORESOURCE_MEM,
	}
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ram_console_resource),
	.resource = ram_console_resource,
};

static int __init setup_ram_console_mem(char *str)
{
	unsigned size = memparse(str, &str);

	if (size && (*str == '@')) {
		unsigned long long base = 0;

		base = simple_strtoul(++str, &str, 0);
		if (reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
			pr_err("%s: failed reserving size %d "
			       "at base 0x%llx\n", __func__, size, base);
			return -1;
		}

		ram_console_resource[0].start = base;
		ram_console_resource[0].end = base + size - 1;
		pr_err("%s: %x at %llx\n", __func__, size, base);
	}
	return 0;
}

__setup("ram_console=", setup_ram_console_mem);
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
static struct samsung_battery_platform_data samsung_battery_pdata = {
	.charger_name	= "max77693-charger",
	.fuelgauge_name	= "max17047-fuelgauge",
	.voltage_max = 4350000,
	.voltage_min = 3400000,


	.in_curr_limit = 1000,
	.chg_curr_ta = 1000,
	.chg_curr_dock = 1000,
	.chg_curr_siop_lv1 = 475,
	.chg_curr_siop_lv2 = 475,
	.chg_curr_siop_lv3 = 475,

	.chg_curr_usb = 475,
	.chg_curr_cdp = 1000,
	.chg_curr_wpc = 475,
	.chg_curr_etc = 475,

	.chng_interval = 30,
	.chng_susp_interval = 60,
	.norm_interval = 120,
	.norm_susp_interval = 7200,
	.emer_lv1_interval = 30,
	.emer_lv2_interval = 10,

	/* it will be cacaluated in probe */
	.recharge_voltage = 4300000,


	.abstimer_charge_duration = 6 * 60 * 60,
	.abstimer_charge_duration_wpc = 8 * 60 * 60,
	.abstimer_recharge_duration = 1.5 * 60 * 60,
	.cb_det_src = CABLE_DET_CHARGER,

	/* M0 EUR */
	.overheat_stop_temp = 600,
	.overheat_recovery_temp = 400,
	.freeze_stop_temp = -50,
	.freeze_recovery_temp = 0,

	/* CTIA spec */
	.ctia_spec  = false,

	/* CTIA temperature spec */
	.event_time = 10 * 60,
	
	/* USA default */
	.event_overheat_stop_temp = 600,
	.event_overheat_recovery_temp = 400,
	.event_freeze_stop_temp = -50,
	.event_freeze_recovery_temp = 0,
	.lpm_overheat_stop_temp = 480,
	.lpm_overheat_recovery_temp = 450,
	.lpm_freeze_stop_temp = -50,
	.lpm_freeze_recovery_temp = 0,

	.temper_src = TEMPER_AP_ADC,
	.temper_ch = 2, /* if src == TEMPER_AP_ADC */
#ifdef CONFIG_S3C_ADC
	/*
	 * s3c adc driver does not convert raw adc data.
	 * so, register convert function.
	 */
	.covert_adc = convert_adc,
#endif

	.vf_det_src = VF_DET_CHARGER,
	.vf_det_ch = 0,	/* if src == VF_DET_ADC */
	.vf_det_th_l = 100,
	.vf_det_th_h = 600,

	.suspend_chging = true,

	.led_indicator = false,

	.battery_standever = false,
};

static struct platform_device samsung_device_battery = {
	.name	= "samsung-battery",
	.id	= -1,
	.dev.platform_data = &samsung_battery_pdata,
};
#endif

#define GPIO_KEYS(_code, _gpio, _active_low, _iswake, _hook)	\
	{							\
		.code = _code,					\
		.gpio = _gpio,					\
		.active_low = _active_low,			\
		.type = EV_KEY,					\
		.wakeup = _iswake,				\
		.debounce_interval = 10,			\
		.isr_hook = _hook,				\
		.value = 1					\
	}

struct gpio_keys_button midas_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
};

struct gpio_keys_button m0_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
};

struct gpio_keys_button m0_rev11_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_HOMEPAGE, GPIO_OK_KEY_ANDROID,
		  1, 1, sec_debug_check_crash_key),
};

struct gpio_keys_platform_data midas_gpiokeys_platform_data = {
	midas_buttons,
	ARRAY_SIZE(midas_buttons),
};

static struct platform_device midas_keypad = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &midas_gpiokeys_platform_data,
	},
};



#ifdef CONFIG_VIDEO_FIMG2D
static struct fimg2d_platdata fimg2d_data __initdata = {
	.hw_ver = 0x41,
	.parent_clkname = "mout_g2d0",
	.clkname = "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate = 199 * 1000000,	/* 160 Mhz */
};
#endif

/* BUSFREQ to control memory/bus */
static struct device_domain busfreq;

static struct platform_device exynos4_busfreq = {
	.id = -1,
	.name = "exynos-busfreq",
};

static struct i2c_gpio_platform_data i2c9_platdata = {
	.sda_pin	= GPIO_RGB_SDA_1_8V,
	.scl_pin	= GPIO_RGB_SCL_1_8V,
	.udelay	= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c9 = {
	.name	= "i2c-gpio",
	.id	= 9,
	.dev.platform_data	= &i2c9_platdata,
};

#ifdef CONFIG_SENSORS_AK8975C
static struct i2c_gpio_platform_data i2c10_platdata = {
	.sda_pin	= GPIO_MSENSOR_SDA_18V,
	.scl_pin	= GPIO_MSENSOR_SCL_18V,
	.udelay	= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c10 = {
	.name	= "i2c-gpio",
	.id	= 10,
	.dev.platform_data	= &i2c10_platdata,
};
#endif

#ifdef CONFIG_SENSORS_LPS331
static struct i2c_gpio_platform_data i2c11_platdata = {
	.sda_pin	= GPIO_BSENSE_SDA_18V,
	.scl_pin	= GPIO_BENSE_SCL_18V,
	.udelay	= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c11 = {
	.name			= "i2c-gpio",
	.id	= 11,
	.dev.platform_data	= &i2c11_platdata,
};
#endif

#if defined(CONFIG_PN65N_NFC) && \
	!defined(CONFIG_MACH_C1) && !defined(CONFIG_MACH_BAFFIN)
static struct i2c_gpio_platform_data i2c12_platdata = {
	.sda_pin		= GPIO_NFC_SDA_18V,
	.scl_pin		= GPIO_NFC_SCL_18V,
	.udelay			= 2, /* 250 kHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c12 = {
	.name	= "i2c-gpio",
	.id		= 12,
	.dev.platform_data	= &i2c12_platdata,
};
#endif

#ifdef CONFIG_USB_HOST_NOTIFY
static void otg_accessory_power(int enable)
{
	u8 on = (u8)!!enable;
	int err;

	/* max77693 otg power control */
	otg_control(enable);

	err = gpio_request(GPIO_OTG_EN, "USB_OTG_EN");
	if (err)
		printk(KERN_ERR "failed to request USB_OTG_EN\n");
	gpio_direction_output(GPIO_OTG_EN, on);
	gpio_free(GPIO_OTG_EN);
	pr_info("%s: otg accessory power = %d\n", __func__, on);
}

static void otg_accessory_powered_booster(int enable)
{
	u8 on = (u8)!!enable;

	/* max77693 powered otg power control */
	powered_otg_control(enable);
	pr_info("%s: otg accessory power = %d\n", __func__, on);
}

static struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.booster	= otg_accessory_power,
	.powered_booster = otg_accessory_powered_booster,
	.thread_enable	= 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};
#endif

#ifdef CONFIG_SEC_WATCHDOG_RESET
static struct platform_device watchdog_reset_device = {
	.name = "watchdog-reset",
	.id = -1,
};
#endif

#ifdef CONFIG_CPUPOWER 
#include <linux/power/cpupower.h> 
static unsigned int table_default_power[1] = { 
	1024 
}; 

static struct cputopo_power default_cpu_power = { 
	.max  = 1, 
	.step = 1, 
	.table = table_default_power, 
}; 
 
static unsigned int table_exynos4412_power[17] = { 
	/* Power save mode CA9 MP */ 
	8192, /*  200 MHz */
	8192, /*  300 MHz */
	8192, /*  400 MHz */
	8192, /*  500 MHz */
	1024, /*  600 MHz */
	1024, /*  700 MHz */
	1024, /*  800 MHz */
	1024, /*  900 MHz */
	1024, /* 1000 MHz */
	1024, /* 1100 MHz */   
	1024, /* 1200 MHz */
	1024, /* 1300 MHz */
	1024, /* 1400 MHz */
	1024, /* 1500 MHz */
	1024, /* 1600 MHz */
	1024, /* 1700 MHz */
	1024, /* 1800 MHz */
}; 
 
static struct cputopo_power exynos4412_cpu_power = { 
	.max  = 17, 
	.step = 100000, 
	.table = table_exynos4412_power, 
}; 
 
/* This table list all possible cpu power configuration */ 
static struct cputopo_power *midas_cpupower_data[2] = { 
	&default_cpu_power, 
	&exynos4412_cpu_power, 
}; 
 
static struct platform_device midas_cpupower_dev = { 
	.name = "cpupower", 
	.dev = { 
		.platform_data = midas_cpupower_data, 
	}, 
}; 
#endif 

static struct platform_device *midas_devices[] __initdata = {
#ifdef CONFIG_SEC_WATCHDOG_RESET
	&watchdog_reset_device,
#endif
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif
	
#ifdef CONFIG_CPUPOWER 
	&midas_cpupower_dev, 
#endif 
	/* Samsung Power Domain */
	&exynos4_device_pd[PD_MFC],
	&exynos4_device_pd[PD_G3D],
	&exynos4_device_pd[PD_LCD0],
	&exynos4_device_pd[PD_CAM],
	&exynos4_device_pd[PD_TV],
	&exynos4_device_pd[PD_GPS],
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_pd[PD_ISP],
#endif
	&exynos4_device_pd[PD_GPS_ALIVE],
	/* legacy fimd */
#ifdef CONFIG_FB_S5P
	&s3c_device_fb,
#endif

#ifdef CONFIG_FB_S5P_MDNIE
	&mdnie_device,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif

#ifdef CONFIG_SND_SOC_WM8994
	&vbatt_device,
#endif

	&s3c_device_wdt,
	&s3c_device_rtc,

	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c3,
#ifdef CONFIG_S3C_DEV_I2C4
	&s3c_device_i2c4,
#endif
	/* &s3c_device_i2c5, */

	&s3c_device_i2c7,
	&s3c_device_i2c8,
	&s3c_device_i2c9,
#ifdef CONFIG_SENSORS_AK8975C
	&s3c_device_i2c10,
#endif

#ifdef CONFIG_SENSORS_LPS331
	&s3c_device_i2c11,
#endif
	/* &s3c_device_i2c12, */
#ifdef CONFIG_BATTERY_MAX17047_FUELGAUGE
	&s3c_device_i2c14,	/* max17047-fuelgauge */
#endif

#ifdef CONFIG_SAMSUNG_MHL
	&s3c_device_i2c15,
#if !defined(CONFIG_MACH_T0) && !defined(CONFIG_MACH_M3)
	&s3c_device_i2c16,
#endif
#endif /* CONFIG_SAMSUNG_MHL */
#if defined(CONFIG_MACH_C1) || \
	defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_M0) || \
	defined(CONFIG_MACH_GC1) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_BAFFIN)
	&s3c_device_i2c17,
#endif
#if defined(CONFIG_LEDS_AN30259A) || defined(CONFIG_LEDS_LP5521)
	&s3c_device_i2c21,
#endif

#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#if defined(CONFIG_USB_ANDROID) || defined(CONFIG_USB_G_ANDROID)
	&s3c_device_android_usb,
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_EXYNOS4_DEV_MSHC
	&s3c_device_mshci,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif

#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos_device_i2s0,
#endif
#if defined(CONFIG_SND_SAMSUNG_RP) || defined(CONFIG_SND_SAMSUNG_ALP)
	&exynos_device_srp,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_fimc_is,
#endif
#ifdef CONFIG_VIDEO_TVOUT
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif
#ifdef CONFIG_FB_S5P_EXTDSP
	&s3c_device_extdsp,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_fimc3,
#endif
#if defined(CONFIG_VIDEO_FIMC_MIPI)
	&s3c_device_csis0,
	&s3c_device_csis1,
#endif
#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	&s5p_device_mfc,
#endif
#ifdef CONFIG_S5P_SYSTEM_MMU
	&SYSMMU_PLATDEV(g2d_acp),
	&SYSMMU_PLATDEV(fimc0),
	&SYSMMU_PLATDEV(fimc1),
	&SYSMMU_PLATDEV(fimc2),
	&SYSMMU_PLATDEV(fimc3),
	&SYSMMU_PLATDEV(jpeg),
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
	&SYSMMU_PLATDEV(tv),
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&SYSMMU_PLATDEV(is_isp),
	&SYSMMU_PLATDEV(is_drc),
	&SYSMMU_PLATDEV(is_fd),
	&SYSMMU_PLATDEV(is_cpu),
#endif
#endif
#ifdef CONFIG_ION_EXYNOS
	&exynos_device_ion,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	&exynos_device_flite0,
	&exynos_device_flite1,
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	&s5p_device_fimg2d,
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
	&s5p_device_jpeg,
#endif
	&samsung_asoc_dma,
#ifndef CONFIG_SND_SOC_SAMSUNG_USE_DMA_WRAPPER
	&samsung_asoc_idma,
#endif
#if defined(CONFIG_S3C64XX_DEV_SPI)
#if defined(CONFIG_VIDEO_S5C73M3_SPI)
	&exynos_device_spi1,
#endif
#endif
#ifdef CONFIG_BT_BCM4334
	&bcm4334_bluetooth_device,
#endif
	&exynos4_busfreq,
#ifdef CONFIG_USB_HOST_NOTIFY
	&host_notifier_device,
#endif
#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
	&s5p_device_tmu,
#endif
};

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
/* below temperature base on the celcius degree */
struct s5p_platform_tmu midas_tmu_data __initdata = {
	.ts = {
		.stop_1st_throttle  = 78,
		.start_1st_throttle = 80,
		.stop_2nd_throttle  = 87,
		.start_2nd_throttle = 103,
		.start_tripping	    = 110, /* temp to do tripping */
		.start_emergency    = 120, /* To protect chip,forcely kernel panic */
		.stop_mem_throttle  = 80,
		.start_mem_throttle = 85,
		.stop_tc  = 13,
		.start_tc = 10,
	},
	.cpufreq = {
		.limit_1st_throttle  = 800000, /* 800MHz in KHz order */
		.limit_2nd_throttle  = 200000, /* 200MHz in KHz order */
	},
	.temp_compensate = {
		.arm_volt = 925000, /* vdd_arm in uV for temperature compensation */
		.bus_volt = 900000, /* vdd_bus in uV for temperature compensation */
		.g3d_volt = 900000, /* vdd_g3d in uV for temperature compensation */
	},
};
#endif

#if defined CONFIG_USB_OHCI_S5P
#if defined(CONFIG_LINK_DEVICE_HSIC) || defined(CONFIG_LINK_DEVICE_USB)
static int __init s5p_ohci_device_initcall(void)
{
	return platform_device_register(&s5p_device_ohci);
}
late_initcall(s5p_ohci_device_initcall);
#endif
#endif

#if defined CONFIG_USB_EHCI_S5P
#if defined(CONFIG_LINK_DEVICE_HSIC) || defined(CONFIG_LINK_DEVICE_USB)
static int __init s5p_ehci_device_initcall(void)
{
	return platform_device_register(&s5p_device_ehci);
}
late_initcall(s5p_ehci_device_initcall);
#endif
#endif

#if defined(CONFIG_VIDEO_TVOUT)
static struct s5p_platform_hpd hdmi_hpd_data __initdata = {

};
static struct s5p_platform_cec hdmi_cec_data __initdata = {

};
#endif

#if defined(CONFIG_CMA)
static unsigned long fbmem_start;
static unsigned long fbmem_size;
static int __init early_fbmem(char *p)
{
	char *endp;

	if (!p)
		return -EINVAL;

	fbmem_size = memparse(p, &endp);
	if (*endp == '@')
		fbmem_start = memparse(endp + 1, &endp);

	return endp > p ? 0 : -EINVAL;
}
early_param("fbmem", early_fbmem);

static void __init exynos4_reserve_mem(void)
{
	static struct cma_region regions[] = {
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
		{
			.name = "fimc_is",
			.size = CONFIG_VIDEO_EXYNOS_MEMSIZE_FIMC_IS * SZ_1K,
			{
				.alignment = 1 << 26,
			},
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD
		{
			.name = "fimd",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD * SZ_1K,
			{
				.alignment = 1 << 20,
			},
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC0
		{
			.name = "fimc0",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC0 * SZ_1K,
			{
				.alignment = 1 << 20,
			},
			.start = 0
		},
#endif
#ifdef CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP
		{
			.name = "srp",
			.size = CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP * SZ_1K,
			.start = 0,
		},
#endif

#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL
		{
			.name = "mfc-normal",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL * SZ_1K,
			.start = 0x64000000,
		},
#endif
		{
			.size = 0
		},
	};
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	static struct cma_region regions_secure[] = {
#if defined(CONFIG_USE_MFC_CMA) && defined(CONFIG_MACH_M0)
#ifdef CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
			.start = 0x5F200000,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE
		{
			.name = "mfc-secure",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE * SZ_1K,
			.start = 0x5C100000,
		},
#endif
		{
			.name = "sectbl",
			.size = SZ_1M,
			.start = 0x5C000000,
		},
#endif
		{
			.size = 0
		},
	};

	static const char map[] __initconst =
		"s3cfb.0=fimd;exynos4-fb.0=fimd;"
		"s3c-fimc.0=fimc0;s3c-fimc.1=fimc1;s3c-fimc.2=fimc2;s3c-fimc.3=fimc3;"
		"exynos4210-fimc.0=fimc0;exynos4210-fimc.1=fimc1;exynos4210-fimc.2=fimc2;exynos4210-fimc.3=fimc3;"
#ifdef CONFIG_ION_EXYNOS
		"ion-exynos=ion;"
#endif
#ifdef CONFIG_VIDEO_MFC5X
		"s3c-mfc/A=mfc0,mfc-secure;"
		"s3c-mfc/B=mfc1,mfc-normal;"
		"s3c-mfc/AB=mfc;"
#endif
		"samsung-rp=srp;"
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
		"exynos4-fimc-is=fimc_is;"
#endif
		"s5p-fimg2d=fimg2d;"
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		"s5p-smem/sectbl=sectbl;"
#endif
		"s5p-smem/mfc=mfc-secure;"
		"s5p-smem/fimc=ion;"
		"s5p-smem/mfc-shm=mfc-normal;"
		"s5p-smem/fimd=fimd;"
		"s5p-smem/fimc0=fimc0";

	int i;

	s5p_cma_region_reserve(regions, regions_secure, 0, map);

	if (!(fbmem_start && fbmem_size))
		return;

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		if (regions[i].name && !strcmp(regions[i].name, "fimd")) {
			memcpy(phys_to_virt(regions[i].start), phys_to_virt(fbmem_start), fbmem_size * SZ_1K);
			printk(KERN_INFO "Bootloader sent 'fbmem' : %08X\n", (u32)fbmem_start);
			break;
		}
	}
}
#endif

static void __init midas_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(smdk4212_uartcfgs, ARRAY_SIZE(smdk4212_uartcfgs));

#if defined(CONFIG_S5P_MEM_CMA)
	exynos4_reserve_mem();
#endif

	/* as soon as INFORM6 is visible, sec_debug is ready to run */
	sec_debug_init();
}

static void __init exynos_sysmmu_init(void)
{
	ASSIGN_SYSMMU_POWERDOMAIN(fimc0, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc1, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc2, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc3, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(jpeg, &exynos4_device_pd[PD_CAM].dev);

#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_l, &exynos4_device_pd[PD_MFC].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_r, &exynos4_device_pd[PD_MFC].dev);
#endif
	ASSIGN_SYSMMU_POWERDOMAIN(tv, &exynos4_device_pd[PD_TV].dev);
#ifdef CONFIG_VIDEO_FIMG2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(g2d_acp).dev, &s5p_device_fimg2d.dev);
#endif
#ifdef CONFIG_VIDEO_MFC5X
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_l).dev, &s5p_device_mfc.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_r).dev, &s5p_device_mfc.dev);
#endif
#ifdef CONFIG_VIDEO_FIMC
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc0).dev, &s3c_device_fimc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc1).dev, &s3c_device_fimc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc2).dev, &s3c_device_fimc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc3).dev, &s3c_device_fimc3.dev);
#endif
#ifdef CONFIG_VIDEO_TVOUT
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_tvout.dev);
#endif
#ifdef CONFIG_VIDEO_JPEG_V2X
	sysmmu_set_owner(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	ASSIGN_SYSMMU_POWERDOMAIN(is_isp, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_drc, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_fd, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_cpu, &exynos4_device_pd[PD_ISP].dev);

	sysmmu_set_owner(&SYSMMU_PLATDEV(is_isp).dev,
		&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_drc).dev,
		&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_fd).dev,
		&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_cpu).dev,
		&exynos4_device_fimc_is.dev);
#endif
}

#ifdef CONFIG_FB_S5P_EXTDSP
struct platform_device s3c_device_extdsp = {
	.name		= "s3cfb_extdsp",
	.id		= 0,
};

static struct s3cfb_extdsp_lcd dummy_buffer = {
	.width = 1920,
	.height = 1080,
	.bpp = 16,
};

static struct s3c_platform_fb default_extdsp_data __initdata = {
	.hw_ver		= 0x70,
	.nr_wins	= 1,
	.default_win	= 0,
	.swap		= FB_SWAP_WORD | FB_SWAP_HWORD,
	.lcd		= &dummy_buffer
};

void __init s3cfb_extdsp_set_platdata(struct s3c_platform_fb *pd)
{
	struct s3c_platform_fb *npd;
	int i;

	if (!pd)
		pd = &default_extdsp_data;

	npd = kmemdup(pd, sizeof(struct s3c_platform_fb), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
	else {
		for (i = 0; i < npd->nr_wins; i++)
			npd->nr_buffers[i] = 1;
		s3c_device_extdsp.dev.platform_data = npd;
	}
}
#endif

static inline int need_i2c5(void)
{
#if defined(CONFIG_MACH_C1) || defined(CONFIG_MACH_M0)
	return system_rev != 3;
#endif
}

static void __init midas_machine_init(void)
{
	struct clk *ppmu_clk = NULL;

#if defined(CONFIG_S3C64XX_DEV_SPI)
#if defined(CONFIG_VIDEO_S5C73M3_SPI)
	unsigned int gpio;
	struct clk *sclk = NULL;
	struct clk *prnt = NULL;
	struct device *spi1_dev = &exynos_device_spi1.dev;
#endif
#endif

	/*
	  * prevent 4x12 ISP power off problem
	  * ISP_SYS Register has to be 0 before ISP block power off.
	  */
	__raw_writel(0x0, S5P_CMU_RESET_ISP_SYS);

	/* initialise the gpios */
	midas_config_gpio_table();
	exynos4_sleep_gpio_table_set = midas_config_sleep_gpio_table;

	midas_power_init();

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	s3c_i2c3_set_platdata(NULL);
	midas_tsp_init();
#ifndef CONFIG_TOUCHSCREEN_MELFAS_GC
	midas_tsp_set_lcdtype(lcdtype);
#endif

#ifdef CONFIG_LEDS_AAT1290A
	platform_device_register(&s3c_device_aat1290a_led);
#endif

#ifdef CONFIG_S3C_DEV_I2C4

	s3c_i2c4_set_platdata(NULL);
	if (!(system_rev != 3 && system_rev >= 0)) {
		i2c_register_board_info(4, i2c_devs4_max77693,
			ARRAY_SIZE(i2c_devs4_max77693));
	}
#endif /* CONFIG_S3C_DEV_I2C4 */
	midas_sound_init();

#ifdef CONFIG_S3C_DEV_I2C5

	if (need_i2c5()) {
		s3c_i2c5_set_platdata(&default_i2c5_data);
		i2c_register_board_info(5, i2c_devs5,
			ARRAY_SIZE(i2c_devs5));
	}
#endif

	s3c_i2c7_set_platdata(NULL);
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	touchkey_init_hw();
#endif
	i2c_register_board_info(8, i2c_devs8_emul, ARRAY_SIZE(i2c_devs8_emul));

	i2c_register_board_info(9, i2c_devs9_emul, ARRAY_SIZE(i2c_devs9_emul));

	i2c_register_board_info(10, i2c_devs10_emul,
				ARRAY_SIZE(i2c_devs10_emul));

	i2c_register_board_info(11, i2c_devs11_emul,
				ARRAY_SIZE(i2c_devs11_emul));

#if defined(CONFIG_PN65N_NFC) && \
	!defined(CONFIG_MACH_C1) && !defined(CONFIG_MACH_BAFFIN)
	i2c_register_board_info(12, i2c_devs12_emul,
				ARRAY_SIZE(i2c_devs12_emul));
#endif

#ifdef CONFIG_BATTERY_MAX17047_FUELGAUGE
	/* max17047 fuel gauge */
	i2c_register_board_info(14, i2c_devs14_emul,
				ARRAY_SIZE(i2c_devs14_emul));
#endif
#ifdef CONFIG_SAMSUNG_MHL
	printk(KERN_INFO "%s() register sii9234 driver\n", __func__);

	i2c_register_board_info(15, i2c_devs15_emul,
				ARRAY_SIZE(i2c_devs15_emul));
	i2c_register_board_info(16, i2c_devs16_emul,
				ARRAY_SIZE(i2c_devs16_emul));
#endif
#if defined(CONFIG_MACH_C1) || \
	defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_M0) || \
	defined(CONFIG_MACH_GC1) || defined(CONFIG_MACH_T0) ||\
	defined(CONFIG_MACH_BAFFIN)

	i2c_register_board_info(17, i2c_devs17_emul,
				ARRAY_SIZE(i2c_devs17_emul));
#endif

#if defined(CONFIG_LEDS_AN30259A) || defined(CONFIG_LEDS_LP5521)
	i2c_register_board_info(21, i2c_devs21_emul,
				ARRAY_SIZE(i2c_devs21_emul));
#endif

#ifdef CONFIG_FB_S5P

#if defined(CONFIG_FB_S5P_MIPI_DSIM)
	mipi_fb_init();
#endif
	s3cfb_set_platdata(&fb_platform_data);

#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fb.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif
#ifdef CONFIG_USB_EHCI_S5P
	smdk4212_ehci_init();
#endif
#ifdef CONFIG_USB_OHCI_S5P
	smdk4212_ohci_init();
#endif
#ifdef CONFIG_USB_GADGET
	smdk4212_usbgadget_init();
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	exynos4_fimc_is_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	exynos4_device_fimc_is.dev.parent = &exynos4_device_pd[PD_ISP].dev;
#endif
#endif
#ifdef CONFIG_EXYNOS4_DEV_MSHC
	s3c_mshci_set_platdata(&exynos4_mshc_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_sdhci2_set_platdata(&smdk4212_hsmmc2_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_sdhci3_set_platdata(&smdk4212_hsmmc3_pdata);
#endif

	midas_camera_init();

#ifdef CONFIG_FB_S5P_EXTDSP
	s3cfb_extdsp_set_platdata(&default_extdsp_data);
#endif

#if defined(CONFIG_VIDEO_TVOUT)
	s5p_hdmi_hpd_set_platdata(&hdmi_hpd_data);
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_tvout.dev.parent = &exynos4_device_pd[PD_TV].dev;
	exynos4_device_pd[PD_TV].dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_jpeg.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	exynos4_jpeg_setup_clock(&s5p_device_jpeg.dev, 160000000);
#endif
#endif

#ifdef CONFIG_ION_EXYNOS
	exynos_ion_set_platdata();
#endif

#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mfc.dev.parent = &exynos4_device_pd[PD_MFC].dev;
#endif
	exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 200 * MHZ);
#endif

#ifdef CONFIG_VIDEO_FIMG2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#endif
	
	brcm_wlan_init();

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
	s5p_tmu_set_platdata(&midas_tmu_data);
#endif

	exynos_sysmmu_init();

	platform_add_devices(midas_devices, ARRAY_SIZE(midas_devices));

#ifdef CONFIG_S3C_ADC
	if (system_rev != 3)
		platform_device_register(&s3c_device_adc);
#endif

#endif
#if defined(CONFIG_BATTERY_SAMSUNG)
	platform_device_register(&samsung_device_battery);
#endif
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
#if defined(CONFIG_MACH_M0_CTC)
	if (system_rev != 3 && system_rev >= 1) {
		midas_gpiokeys_platform_data.buttons = m0_buttons;
		midas_gpiokeys_platform_data.nbuttons = ARRAY_SIZE(m0_buttons);
	}
#endif
	/* Above logic is too complex. Let's override whatever the
	   result is... */

#if defined(CONFIG_MACH_M0)
	if (system_rev >= 11) {
		s3c_gpio_setpull(GPIO_OK_KEY_ANDROID, S3C_GPIO_PULL_NONE);
		midas_gpiokeys_platform_data.buttons = m0_rev11_buttons;
		midas_gpiokeys_platform_data.nbuttons =
			ARRAY_SIZE(m0_rev11_buttons);
	}
#endif

	platform_device_register(&midas_keypad);

#if defined(CONFIG_S3C_DEV_I2C5)
	if (need_i2c5())
		platform_device_register(&s3c_device_i2c5);
#endif

#if defined(CONFIG_PN65N_NFC) && \
	!defined(CONFIG_MACH_C1) && !defined(CONFIG_MACH_BAFFIN)
	platform_device_register(&s3c_device_i2c12);
#endif

#if defined(CONFIG_S3C64XX_DEV_SPI)
#if defined(CONFIG_VIDEO_S5C73M3_SPI)
	sclk = clk_get(spi1_dev, "dout_spi1");
	if (IS_ERR(sclk))
		dev_err(spi1_dev, "failed to get sclk for SPI-1\n");
	prnt = clk_get(spi1_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi1_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
		       prnt->name, sclk->name);

	clk_set_rate(sclk, 100 * 1000 * 1000); /*50MHz*/

	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPB(5), "SPI_CS1")) {
		gpio_direction_output(EXYNOS4_GPB(5), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPB(5), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPB(5), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(1, EXYNOS_SPI_SRCCLK_SCLK,
				    ARRAY_SIZE(spi1_csi));
	}

	for (gpio = EXYNOS4_GPB(4); gpio < EXYNOS4_GPB(8); gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	spi_register_board_info(spi1_board_info, ARRAY_SIZE(spi1_board_info));
#endif
#endif

#ifdef CONFIG_BUSFREQ_OPP
	dev_add(&busfreq, &exynos4_busfreq.dev);

	/* PPMUs using for cpufreq get clk from clk_list */
	ppmu_clk = clk_get(NULL, "ppmudmc0");
	if (IS_ERR(ppmu_clk))
		printk(KERN_ERR "failed to get ppmu_dmc0\n");
	clk_enable(ppmu_clk);
	clk_put(ppmu_clk);

	ppmu_clk = clk_get(NULL, "ppmudmc1");
	if (IS_ERR(ppmu_clk))
		printk(KERN_ERR "failed to get ppmu_dmc1\n");
	clk_enable(ppmu_clk);
	clk_put(ppmu_clk);

	ppmu_clk = clk_get(NULL, "ppmucpu");
	if (IS_ERR(ppmu_clk))
		printk(KERN_ERR "failed to get ppmu_cpu\n");
	clk_enable(ppmu_clk);
	clk_put(ppmu_clk);

	ppmu_init(&exynos_ppmu[PPMU_DMC0], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_DMC1], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_CPU], &exynos4_busfreq.dev);
#endif

	/* 400 kHz for initialization of MMC Card  */
	__raw_writel((__raw_readl(EXYNOS4_CLKDIV_FSYS3) & 0xfffffff0)
		     | 0x9, EXYNOS4_CLKDIV_FSYS3);

	__raw_writel((__raw_readl(EXYNOS4_CLKDIV_FSYS2) & 0xfff0fff0)
		     | 0x80008, EXYNOS4_CLKDIV_FSYS2);

	__raw_writel((__raw_readl(EXYNOS4_CLKDIV_FSYS1) & 0xfff0fff0)
		     | 0x80008, EXYNOS4_CLKDIV_FSYS1);
}


#ifdef CONFIG_DMA_CMA
static void __init exynos4_reserve(void)
{
	int ret = 0;

#ifdef CONFIG_USE_FIMC_CMA
	ret = dma_declare_contiguous(&s3c_device_fimc1.dev,
		CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC1 * SZ_1K, 0x65800000, 0);
	if (ret != 0)
		panic("alloc failed for FIMC1\n");
#endif

#if defined(CONFIG_USE_MFC_CMA) && defined(CONFIG_MACH_M0)
	ret = dma_declare_contiguous(&s5p_device_mfc.dev,
			0x02800000, 0x5C800000, 0);
#endif
	if (ret != 0)
		printk(KERN_ERR "%s Fail\n", __func__);
}
#endif

static void __init exynos_init_reserve(void)
{
	sec_debug_magic_init();
}

MACHINE_START(SMDK4412, "SMDK4x12")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= midas_map_io,
	.init_machine	= midas_machine_init,
	.timer		= &exynos4_timer,
	.reserve	= &exynos4_reserve,
	.init_early	= &exynos_init_reserve,
MACHINE_END

MACHINE_START(SMDK4212, "SMDK4x12")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= midas_map_io,
	.init_machine	= midas_machine_init,
	.timer		= &exynos4_timer,
	.reserve	= &exynos4_reserve,
	.init_early	= &exynos_init_reserve,
MACHINE_END
