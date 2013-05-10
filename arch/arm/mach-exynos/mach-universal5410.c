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
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/persistent_ram.h>
#include <linux/gpio.h>
#include <linux/cma.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/memblock.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/system_info.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/exynos_fiq_debugger.h>
#include <mach/map.h>
#include <mach/hs-iic.h>
#include <mach/regs-pmu.h>

#include "../../../drivers/staging/android/ram_console.h"
#include "common.h"
#include "board-universal5410.h"
#include "resetreason.h"
#include <linux/i2c-gpio.h>
#include <mach/gpio-exynos.h>

#include "board-universal5410-wlan.h"
#include "include/board-bluetooth-bcm.h"

#include "mach/sec_debug.h"

extern phys_addr_t bootloaderfb_start;
extern phys_addr_t bootloaderfb_size;

#if defined(CONFIG_FELICA)
//#include <mach/gpio-exynos5410-lte-jpn-rev00.h>
#define  FELICA_GPIO_RFS_NAME     "FeliCa-RFS"
#define  FELICA_GPIO_PON_NAME     "FeliCa-PON"
#define  FELICA_GPIO_INT_NAME     "FeliCa-INT"
#define  FELICA_GPIO_I2C_SDA_NAME "FeliCa-SDA"
#define  FELICA_GPIO_I2C_SCL_NAME "FeliCa-SCL"
#define  SNFC_GPIO_HSEL_NAME      "SNFC-HSEL"
#define  SNFC_GPIO_INTU_NAME      "SNFC-INTU"

static struct  i2c_gpio_platform_data  i2c30_gpio_platdata = {
	.sda_pin = FELICA_GPIO_I2C_SDA,
	.scl_pin = FELICA_GPIO_I2C_SCL,
	.udelay  = 0,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};
static struct  platform_device  s3c_device_i2c30 = {
	.name  = "i2c-gpio",
	.id   = 30,                               /* adepter number */
	.dev.platform_data = &i2c30_gpio_platdata,
};

static struct i2c_board_info i2c_devs30[] __initdata = {
	{
		I2C_BOARD_INFO("felica_i2c", (0x56 >> 1)),
	},
};
#endif

#ifdef CONFIG_EXYNOS_C2C
#include <mach/c2c.h>
#endif
#ifdef CONFIG_SEC_MODEM
#include <linux/platform_data/modem_if.h>
#endif

static struct ram_console_platform_data ramconsole_pdata;

static struct platform_device ramconsole_device = {
	.name	= "ram_console",
	.id	= -1,
	.dev	= {
		.platform_data = &ramconsole_pdata,
	},
};

static struct platform_device persistent_trace_device = {
	.name	= "persistent_trace",
	.id	= -1,
};

/*rfkill device registeration*/
#ifdef CONFIG_BT_BCM4335
static struct platform_device bcm4335_bluetooth_device = {
	.name = "bcm4335_bluetooth",
	.id = -1,
};
#endif

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK5410_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK5410_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK5410_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg universal5410_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK5410_UCON_DEFAULT,
		.ulcon		= SMDK5410_ULCON_DEFAULT,
		.ufcon		= SMDK5410_UFCON_DEFAULT,
		.wake_peer	= bcm_bt_lpm_exit_lpm_locked,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK5410_UCON_DEFAULT,
		.ulcon		= SMDK5410_ULCON_DEFAULT,
		.ufcon		= SMDK5410_UFCON_DEFAULT,
	},
	[2] = {
#ifndef CONFIG_EXYNOS_FIQ_DEBUGGER
	/*
	 * Don't need to initialize hwport 2, when FIQ debugger is
	 * enabled. Because it will be handled by fiq_debugger.
	 */
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK5410_UCON_DEFAULT,
		.ulcon		= SMDK5410_ULCON_DEFAULT,
		.ufcon		= SMDK5410_UFCON_DEFAULT,
	},
	[3] = {
#endif
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK5410_UCON_DEFAULT,
		.ulcon		= SMDK5410_ULCON_DEFAULT,
		.ufcon		= SMDK5410_UFCON_DEFAULT,
	},
};

static int exynos5_notifier_call(struct notifier_block *this,
		unsigned long code, void *_cmd)
{
	int mode = 0;

	if ((code == SYS_RESTART) && _cmd)
		if (!strcmp((char *)_cmd, "recovery"))
			mode = 0xf;

	__raw_writel(mode, EXYNOS_INFORM4);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_reboot_notifier = {
	.notifier_call = exynos5_notifier_call,
};

static struct persistent_ram_descriptor universal5410_prd[] __initdata = {
	{
		.name = "ram_console",
		.size = SZ_2M,
	},
#ifdef CONFIG_PERSISTENT_TRACER
	{
		.name = "persistent_trace",
		.size = SZ_1M,
	},
#endif
};

static struct persistent_ram universal5410_pr __initdata = {
	.descs		= universal5410_prd,
	.num_descs	= ARRAY_SIZE(universal5410_prd),
	.start		= PLAT_PHYS_OFFSET + SZ_512M,
#ifdef CONFIG_PERSISTENT_TRACER
	.size		= 3 * SZ_1M,
#else
	.size		= SZ_2M,
#endif
};

#if defined(CONFIG_CMA)
#include "reserve-mem.h"
static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
#if defined(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE) && \
			(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE != 0)
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
#endif
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#ifdef CONFIG_ION_EXYNOS_DRM_MFC_SH
		{
			.name = "drm_mfc_sh",
			.size = SZ_1M,
		},
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD
		{
			.name = "drm_g2d_wfd",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD *
				SZ_1K,
		},
#endif
#endif
#ifdef CONFIG_EXYNOS_C2C
		{
			.name = "c2c_shdmem",
			.size = C2C_SHAREDMEM_SIZE,
			{ .alignment = C2C_SHAREDMEM_SIZE, },
		},
#endif
#ifdef CONFIG_BL_SWITCHER
		{
			.name = "bl_mem",
			.size = SZ_8K,
		},
#endif
		{
			.size = 0
		},
	};
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	static struct cma_region regions_secure[] = {
#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_FIMD_VIDEO) && \
			(CONFIG_ION_EXYNOS_DRM_MEMSIZE_FIMD_VIDEO != 0)
	       {
			.name = "drm_fimd_video",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_FIMD_VIDEO *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
	       },
#endif
#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_OUTPUT) && \
			(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_OUTPUT != 0)
	       {
			.name = "drm_mfc_output",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_OUTPUT *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
	       },
#endif
#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT) && \
			(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT != 0)
	       {
			.name = "drm_mfc_input",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
	       },
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_MFC_FW
		{
			.name = "drm_mfc_fw",
			.size = SZ_1M,
			{
				.alignment = SZ_1M,
			}
		},
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_SECTBL
		{
			.name = "drm_sectbl",
			.size = SZ_1M,
			{
				.alignment = SZ_1M,
			}
		},
#endif
		{
			.size = 0
		},
	};
#else /* !CONFIG_EXYNOS_CONTENT_PATH_PROTECTION */
	struct cma_region *regions_secure = NULL;
#endif /* CONFIG_EXYNOS_CONTENT_PATH_PROTECTION */
	static const char map[] __initconst =
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		"ion-exynos/mfc_sh=drm_mfc_sh;"
		"ion-exynos/g2d_wfd=drm_g2d_wfd;"
		"ion-exynos/fimd_video=drm_fimd_video;"
		"ion-exynos/mfc_output=drm_mfc_output;"
		"ion-exynos/mfc_input=drm_mfc_input;"
		"ion-exynos/mfc_fw=drm_mfc_fw;"
		"ion-exynos/sectbl=drm_sectbl;"
		"s5p-smem/mfc_sh=drm_mfc_sh;"
		"s5p-smem/g2d_wfd=drm_g2d_wfd;"
		"s5p-smem/fimd_video=drm_fimd_video;"
		"s5p-smem/mfc_output=drm_mfc_output;"
		"s5p-smem/mfc_input=drm_mfc_input;"
		"s5p-smem/mfc_fw=drm_mfc_fw;"
		"s5p-smem/sectbl=drm_sectbl;"
#endif
#ifdef CONFIG_EXYNOS_C2C
		"samsung-c2c=c2c_shdmem;"
#endif
#ifdef CONFIG_BL_SWITCHER
		"b.L_mem=bl_mem;"
#endif
		"ion-exynos=ion;";
	exynos_cma_region_reserve(regions, regions_secure, NULL, map);
	persistent_ram_early_init(&universal5410_pr);
	if (bootloaderfb_start) {
		int err = memblock_reserve(bootloaderfb_start,
				bootloaderfb_size);
		if (err)
			pr_warn("failed to reserve old framebuffer location\n");
	} else {
		pr_warn("bootloader framebuffer start address not set\n");
	}
}
#else /*!CONFIG_CMA*/
static inline void exynos_reserve_mem(void)
{
	persistent_ram_early_init(&universal5410_pr);
}
#endif

#if defined(CONFIG_UMTS_MODEM_SS222) && defined(CONFIG_LINK_DEVICE_C2C)
struct modem_boot_spi_platform_data modem_boot_spi_pdata = {
	.name = "modem_status",
	.gpio_cp_status = GPIO_LTE2AP_STATUS,
};

static struct spi_board_info modem_spi_board_info[] = {
	{
		.modalias = "modem_boot_spi",
		.controller_data = (void *)GPIO_MODEM_SPI_CSN,
		.platform_data = &modem_boot_spi_pdata,
		.max_speed_hz = (2 * 1000 * 1000),
		.bus_num = 4,
		.chip_select = 0,
		.mode = SPI_MODE_0,
	},
};

static struct spi_gpio_platform_data ss222_spi_gpio_pdata = {
	.sck = GPIO_MODEM_SPI_CLK,
	.mosi = GPIO_MODEM_SPI_MOSI,
	.miso = GPIO_MODEM_SPI_MISO,
	.num_chipselect = 1,
};

static struct platform_device ss222_spi_boot_pdata = {
	.name = "spi_gpio",
	.id = 4,
	.dev = {
		.platform_data = &ss222_spi_gpio_pdata,
	},
};

static void __init modem_spi_init(void)
{
	int err;
	unsigned size;
	int gpio;

	pr_err("%s: +++\n", __func__);
#if 1
	s3c_gpio_cfgpin(EXYNOS5410_GPA2(4), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(EXYNOS5410_GPA2(4), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgall_range(EXYNOS5410_GPA2(6), 2,
			      S3C_GPIO_SFN(2), S3C_GPIO_PULL_UP);

	for (gpio = EXYNOS5410_GPA2(4);
			gpio < EXYNOS5410_GPA2(8); gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
#else
	s3c_gpio_cfgpin(GPIO_MODEM_SPI_CLK, SPI_SFN);
	s3c_gpio_setpull(GPIO_MODEM_SPI_CLK, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_MODEM_SPI_CSN, SPI_SFN);
	s3c_gpio_setpull(GPIO_MODEM_SPI_CSN, S3C_GPIO_PULL_NONE);
#endif

	s3c_gpio_cfgpin(GPIO_MODEM_SPI_MISO, SPI_SFN);
	s3c_gpio_setpull(GPIO_MODEM_SPI_MISO, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_MODEM_SPI_MOSI, SPI_SFN);
	s3c_gpio_setpull(GPIO_MODEM_SPI_MOSI, S3C_GPIO_PULL_NONE);

	size = ARRAY_SIZE(modem_spi_board_info);
	err = spi_register_board_info(modem_spi_board_info, size);
	if (err)
		pr_err("ERR! spi_register_board_info fail (err %d)\n", err);

	pr_err("%s: ---\n", __func__);
}
#endif

static struct platform_device *universal5410_devices[] __initdata = {
	&ramconsole_device,
	&persistent_trace_device,
	&s3c_device_wdt,
	&s3c_device_rtc,
	&s3c_device_adc,
#ifdef CONFIG_PVR_SGX
	&exynos5_device_g3d,
#endif
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_ace,
#endif
#if defined(CONFIG_UMTS_MODEM_SS222) && (CONFIG_LINK_DEVICE_C2C)
	&ss222_spi_boot_pdata,
#endif
#ifdef CONFIG_BT_BCM4335
	&bcm4335_bluetooth_device,
#endif
#if defined(CONFIG_FELICA)
	&s3c_device_i2c30,
#endif
};
static void __init universal5410_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	clk_xxti.rate = 24000000;

	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(universal5410_uartcfgs, ARRAY_SIZE(universal5410_uartcfgs));

	sec_debug_init();
}

static void __init universal5410_init_early(void)
{
	sec_debug_magic_init();
}

static unsigned int board_rev = 0;

unsigned int universal5410_rev(void)
{
	return board_rev;
}

static struct gpio __initdata universal5410_hw_rev[] = {
	{ EXYNOS5410_GPH0(0), GPIOF_DIR_IN, "hw_rev0" },
	{ EXYNOS5410_GPH0(1), GPIOF_DIR_IN, "hw_rev1" },
	{ EXYNOS5410_GPH0(2), GPIOF_DIR_IN, "hw_rev2" },
	{ EXYNOS5410_GPH0(3), GPIOF_DIR_IN, "hw_rev3" },
};

#if 0 // unused
static void __init universal5410_get_board_revision(void)
{
	int err;

	err = gpio_request_array(universal5410_hw_rev,
					ARRAY_SIZE(universal5410_hw_rev));

	if (err) {
		pr_err("%s: failed [%d] to request GPIO\n", __func__, err);
		return;
	}

	board_rev = (gpio_get_value(EXYNOS5410_GPH0(0)) << 0) |
			(gpio_get_value(EXYNOS5410_GPH0(1)) << 1) |
			(gpio_get_value(EXYNOS5410_GPH0(2)) << 2) |
			(gpio_get_value(EXYNOS5410_GPH0(3)) << 3);

	pr_info("Universal_5410 Revision[%d]\n", board_rev);

	gpio_free_array(universal5410_hw_rev,
				ARRAY_SIZE(universal5410_hw_rev));
}
#endif

#if defined(CONFIG_FELICA)
static void felica_setup(void)
{
	/* I2C SDA GPY2[4] */
	gpio_request(FELICA_GPIO_I2C_SDA, FELICA_GPIO_I2C_SDA_NAME);
	s3c_gpio_setpull(FELICA_GPIO_I2C_SDA, S3C_GPIO_PULL_DOWN);
	gpio_free(FELICA_GPIO_I2C_SDA);

	/* I2C SCL GPY2[5] */
	gpio_request(FELICA_GPIO_I2C_SCL, FELICA_GPIO_I2C_SCL_NAME);
	s3c_gpio_setpull(FELICA_GPIO_I2C_SCL, S3C_GPIO_PULL_DOWN);
	gpio_free(FELICA_GPIO_I2C_SCL);

	/* PON GPL2[7] */
	gpio_request(FELICA_GPIO_PON, FELICA_GPIO_PON_NAME);
	s3c_gpio_setpull(FELICA_GPIO_PON, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(FELICA_GPIO_PON, S3C_GPIO_SFN(1)); /* OUTPUT */
	gpio_free(FELICA_GPIO_PON);

	/* RFS GPL2[6] */
#ifdef CONFIG_NFC_FELICA
	gpio_request(FELICA_GPIO_RFS, FELICA_GPIO_RFS_NAME);
	s3c_gpio_setpull(FELICA_GPIO_RFS, S3C_GPIO_PULL_DOWN);
	s5p_register_gpio_interrupt(FELICA_GPIO_RFS);
	gpio_direction_input(FELICA_GPIO_RFS);
	irq_set_irq_type(gpio_to_irq(FELICA_GPIO_RFS), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(FELICA_GPIO_RFS, S3C_GPIO_SFN(0xF)); /* EINT */
	gpio_free(FELICA_GPIO_RFS);
#else
	gpio_request(FELICA_GPIO_RFS, FELICA_GPIO_RFS_NAME);
	s3c_gpio_setpull(FELICA_GPIO_RFS, S3C_GPIO_PULL_DOWN);
	gpio_direction_input(FELICA_GPIO_RFS);
	gpio_free(FELICA_GPIO_RFS);
#endif
	/* INT GPX1[7] = WAKEUP_INT1[7] */
	if(system_rev == 0x01)
	{
		//HW-REV-0.3(0x01)
		gpio_request(FELICA_GPIO_INT_REV03, FELICA_GPIO_INT_NAME);
		s3c_gpio_setpull(FELICA_GPIO_INT_REV03, S3C_GPIO_PULL_DOWN);
		s5p_register_gpio_interrupt(FELICA_GPIO_INT_REV03);
		gpio_direction_input(FELICA_GPIO_INT_REV03);
		irq_set_irq_type(gpio_to_irq(FELICA_GPIO_INT_REV03), IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(FELICA_GPIO_INT_REV03, S3C_GPIO_SFN(0xF)); /* EINT */
		gpio_free(FELICA_GPIO_INT_REV03);
	}
	else
	{
		//HW-REV-0.0(0x0b), HW-REV-0.1(0x09)
		gpio_request(FELICA_GPIO_INT_REV00, FELICA_GPIO_INT_NAME);
		s3c_gpio_setpull(FELICA_GPIO_INT_REV00, S3C_GPIO_PULL_DOWN);
		s5p_register_gpio_interrupt(FELICA_GPIO_INT_REV00);
		gpio_direction_input(FELICA_GPIO_INT_REV00);
		irq_set_irq_type(gpio_to_irq(FELICA_GPIO_INT_REV00), IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(FELICA_GPIO_INT_REV00, S3C_GPIO_SFN(0xF)); /* EINT */
		gpio_free(FELICA_GPIO_INT_REV00);
	}
	
	/* HSEL GPX0[4] */
	gpio_request(SNFC_GPIO_HSEL, SNFC_GPIO_HSEL_NAME);
	s3c_gpio_setpull(SNFC_GPIO_HSEL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(SNFC_GPIO_HSEL, S3C_GPIO_SFN(1)); /* OUTPUT */

	gpio_free(SNFC_GPIO_HSEL);
	
	/* INTU GPX1[5] */
	if(system_rev == 0x01)
	{
		//HW-REV-0.3(0x01)
		gpio_request(SNFC_GPIO_INTU_REV03, SNFC_GPIO_INTU_NAME);
		s3c_gpio_setpull(SNFC_GPIO_INTU_REV03, S3C_GPIO_PULL_DOWN);
		s5p_register_gpio_interrupt(SNFC_GPIO_INTU_REV03);
		gpio_direction_input(SNFC_GPIO_INTU_REV03);
		irq_set_irq_type(gpio_to_irq(SNFC_GPIO_INTU_REV03), IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(SNFC_GPIO_INTU_REV03, S3C_GPIO_SFN(0xF)); /* EINT */
		gpio_free(SNFC_GPIO_INTU_REV03);
	}
	else
	{
		//HW-REV-0.0(0x0b), HW-REV-0.1(0x09)
		gpio_request(SNFC_GPIO_INTU_REV00, SNFC_GPIO_INTU_NAME);
		s3c_gpio_setpull(SNFC_GPIO_INTU_REV00, S3C_GPIO_PULL_DOWN);
		s5p_register_gpio_interrupt(SNFC_GPIO_INTU_REV00);
		gpio_direction_input(SNFC_GPIO_INTU_REV00);
		irq_set_irq_type(gpio_to_irq(SNFC_GPIO_INTU_REV00), IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(SNFC_GPIO_INTU_REV00, S3C_GPIO_SFN(0xF)); /* EINT */
		gpio_free(SNFC_GPIO_INTU_REV00);
	}
}
#endif
static void __init universal5410_machine_init(void)
{
#ifdef CONFIG_EXYNOS_FIQ_DEBUGGER
	exynos_serial_debug_init(2, 0);
#endif

	exynos5_universal5410_clock_init();
	exynos5_universal5410_mmc_init();
	exynos5_universal5410_usb_init();
#ifdef CONFIG_BATTERY_SAMSUNG
#if defined(CONFIG_MACH_V1)
	exynos5_vienna_battery_init();
#else
	exynos5_universal5410_battery_init();
#endif
#endif
	exynos5_universal5410_display_init();
	exynos5_universal5410_input_init();
	exynos5_universal5410_audio_init();
	exynos5_universal5410_power_init();
	exynos5_universal5410_media_init();
#if !defined(CONFIG_MACH_V1)	
	exynos5_universal5410_led_init();
#endif	
	exynos5_universal5410_vibrator_init();
#ifdef CONFIG_ICE4_FPGA
	exynos5_universal5410_fpga_init();
#endif
#ifdef CONFIG_IR_REMOCON_MC96
	v1_irda_init();
#endif
#ifdef CONFIG_EXYNOS_C2C
	exynos5_universal5410_c2c_init();
#endif
#if defined(CONFIG_BCM4335) || defined(CONFIG_BCM4335_MODULE)
	brcm_wlan_init();
#endif
	exynos5_universal5410_mfd_init();

#if defined(CONFIG_UMTS_MODEM_SS222) && (CONFIG_LINK_DEVICE_C2C)
	modem_spi_init();
#endif
	if (lpcharge == 0) {
		exynos5_universal5410_sensor_init();

#ifndef CONFIG_FELICA
#ifdef CONFIG_NFC_DEVICES
		exynos5_universal5410_nfc_init();
#endif
#endif
	} else
		pr_info("[SSP NFC] : Poweroff charging\n");

#ifdef CONFIG_SAMSUNG_MHL_8240
	exynos5_universal5410_mhl_init();
#endif
#if defined(CONFIG_FELICA)
	i2c_register_board_info(30, i2c_devs30, ARRAY_SIZE(i2c_devs30));
	felica_setup();
#endif /* CONFIG_FELICA */

#ifdef CONFIG_STMPE811_ADC
	exynos5_universal5410_stmpe_adc_init();
#endif
#ifdef CONFIG_30PIN_CONN
	exynos5_universal5410_accessory_init();
#endif
#ifdef CONFIG_CORESIGHT_ETM
	exynos5_universal5410_coresight_init();
#endif
	ramconsole_pdata.bootinfo = exynos_get_resetreason();
	platform_add_devices(universal5410_devices, ARRAY_SIZE(universal5410_devices));
	register_reboot_notifier(&exynos5_reboot_notifier);
}

MACHINE_START(UNIVERSAL5410, "UNIVERSAL5410")
	.atag_offset	= 0x100,
	.init_early	= universal5410_init_early,
	.init_irq	= exynos5_init_irq,
	.map_io		= universal5410_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal5410_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos5_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
