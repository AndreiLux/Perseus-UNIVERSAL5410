/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/smsc911x.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/sdhci.h>

#include <mach/dwmci.h>

#include "board-universal5410.h"

#define GPIO_T_FLASH_DETECT	EXYNOS5410_GPX2(4)

static struct dw_mci_clk exynos_dwmci_clk_rates_for_cpll[] = {
	{20 * 1000 * 1000, 80 * 1000 * 1000},
	{40 * 1000 * 1000, 160 * 1000 * 1000},
	{40 * 1000 * 1000, 160 * 1000 * 1000},
	{80 * 1000 * 1000, 320 * 1000 * 1000},
	{160 * 1000 * 1000, 640 * 1000 * 1000},
	{80 * 1000 * 1000, 320 * 1000 * 1000},
	{160 * 1000 * 1000, 640 * 1000 * 1000},
	{320 * 1000 * 1000, 640 * 1000 * 1000},
};

static struct dw_mci_clk exynos_dwmci_clk_rates_for_epll[] = {
	{25 * 1000 * 1000, 50 * 1000 * 1000},
	{50 * 1000 * 1000, 100 * 1000 * 1000},
	{50 * 1000 * 1000, 100 * 1000 * 1000},
	{100 * 1000 * 1000, 200 * 1000 * 1000},
	{200 * 1000 * 1000, 400 * 1000 * 1000},
	{100 * 1000 * 1000, 200 * 1000 * 1000},
	{200 * 1000 * 1000, 400 * 1000 * 1000},
	{400 * 1000 * 1000, 800 * 1000 * 1000},
};

static void exynos_dwmci_save_drv_st(void *data, u32 slot_id)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength * drv_st = &host->pdata->__drv_st;

	drv_st->val = s5p_gpio_get_drvstr(drv_st->pin);
}

static void exynos_dwmci_restore_drv_st(void *data, u32 slot_id, int *compensation)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength * drv_st = &host->pdata->__drv_st;
	s5p_gpio_drvstr_t cur, org;

	/*
		LV1	LV3	LV2	LV4 (org)
	 LV1	0	-1	0	-1
	 LV3	1	0	1	0
	 LV2	0	-1	0	-1
	 LV4	1	0	1	0
	 (cur)
	*/

	int compensation_tbl[4][4] = {
		{0,	-1,	0,	 -1},
		{1,	0,	1,	0},
		{0,	-1,	0,	-1},
		{1,	0,	1,	0}
	};

	cur = s5p_gpio_get_drvstr(drv_st->pin);
	org = drv_st->val;
	s5p_gpio_set_drvstr(drv_st->pin, drv_st->val);
	*compensation = compensation_tbl[cur][org];
}

static void exynos_dwmci_tuning_drv_st(void *data, u32 slot_id)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength * drv_st = &host->pdata->__drv_st;
	unsigned int gpio = drv_st->pin;
	s5p_gpio_drvstr_t next_ds[4] = {S5P_GPIO_DRVSTR_LV4,   /* LV1 -> LV4 */
					S5P_GPIO_DRVSTR_LV2,   /* LV3 -> LV2 */
					S5P_GPIO_DRVSTR_LV1,   /* LV2 -> LV1 */
					S5P_GPIO_DRVSTR_LV3};  /* LV4 -> LV3 */

	s5p_gpio_set_drvstr(gpio, next_ds[s5p_gpio_get_drvstr(gpio)]);
}

static int exynos_dwmci0_get_bus_wd(u32 slot_id)
{
	return 8;
}

static void exynos_dwmci0_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS5410_GPC0(0);
			gpio < EXYNOS5410_GPC0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS5410_GPC3(0);
				gpio <= EXYNOS5410_GPC3(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
	case 4:
		for (gpio = EXYNOS5410_GPC0(3);
				gpio <= EXYNOS5410_GPC0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS5410_GPC0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}

	gpio = EXYNOS5410_GPD1(0);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
}

static struct dw_mci_board universal5410_dwmci0_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 160 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_8_BIT_DATA |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_ERASE,
	.caps2			= MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS200_1_8V_DDR,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci0_cfg_gpio,
	.get_bus_wd		= exynos_dwmci0_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.sdr_timing		= 0x03040000,
	.ddr_timing		= 0x03020000,
	.clk_drv		= 0x3,
	.ddr200_timing		= 0x01020000,
	.clk_tbl		= exynos_dwmci_clk_rates_for_cpll,
	.__drv_st		= {
		.pin			= EXYNOS5410_GPC0(0),
		.val			= S5P_GPIO_DRVSTR_LV4,
	},
	.qos_int_level		= 160 * 1000,
};

static int universal5410_dwmci_get_ro(u32 slot_id)
{
	/* universal5410 rev1.0 did not support SD/MMC card write pritect. */
	return 0;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio, bit_cnt = 0;

	for (gpio = EXYNOS5410_GPC1(0); gpio < EXYNOS5410_GPC1(3); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	gpio = EXYNOS5410_GPD1(1);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);


	for (gpio = EXYNOS5410_GPC1(3); gpio <= EXYNOS5410_GPC1(6);
			gpio++, bit_cnt++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		if (bit_cnt < width)
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		else
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}
	for (gpio = EXYNOS5410_GPD1(4); gpio <= EXYNOS5410_GPD1(7);
			gpio++, bit_cnt++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		if (bit_cnt < width)
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		else
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}
}

static void (*wlan_notify_func)(struct platform_device *dev, int state);
static DEFINE_MUTEX(wlan_mutex_lock);

static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = notify_func;
	mutex_unlock(&wlan_mutex_lock);

	return 0;
}

static int ext_cd_cleanup_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = NULL;
	mutex_unlock(&wlan_mutex_lock);

	return 0;
}

static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 4;
}

static struct dw_mci_board universal5410_dwmci1_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 200 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_SDR104 |
				  MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci1_cfg_gpio,
	.get_bus_wd		= exynos_dwmci1_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.ext_cd_init		= ext_cd_init_wlan,
	.ext_cd_cleanup		= ext_cd_cleanup_wlan,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.sdr_timing		= 0x81020000,
	.ddr_timing		= 0x81020000,
	.clk_drv		= 2,
	.clk_tbl		= exynos_dwmci_clk_rates_for_epll,
	.__drv_st		= {
		.pin			= EXYNOS5410_GPC1(0),
		.val			= S5P_GPIO_DRVSTR_LV4,
	},
	.qos_int_level		= 160 * 1000,
};

static int exynos_dwmci2_get_cd(u32 slot_id)
{
	return gpio_get_value(GPIO_T_FLASH_DETECT);
}

static void exynos_dwmci2_cfg_gpio(int width)
{
	unsigned int gpio;

	s3c_gpio_setpull(GPIO_T_FLASH_DETECT, S3C_GPIO_PULL_UP);

	for (gpio = EXYNOS5410_GPC2(0); gpio < EXYNOS5410_GPC2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 4:
		for (gpio = EXYNOS5410_GPC2(3);
				gpio <= EXYNOS5410_GPC2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS5410_GPC2(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}

	gpio = EXYNOS5410_GPC2(2);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
}

static int exynos_dwmci2_get_bus_wd(u32 slot_id)
{
	if (samsung_rev() < EXYNOS5410_REV_1_0)
		return 1;
	else
		return 4;
}


struct dw_mci *host2;
static int ext_cd_irq = 0;

static int exynos_dwmci2_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct device *dev = &host->dev;

	host2 = host;

	if (host->pdata->cd_type == DW_MCI_CD_GPIO &&
		gpio_is_valid(GPIO_T_FLASH_DETECT)) {

		s3c_gpio_setpull(GPIO_T_FLASH_DETECT, S3C_GPIO_PULL_UP);

		if (gpio_request(GPIO_T_FLASH_DETECT, "DWMCI EXT CD") == 0) {
			ext_cd_irq = gpio_to_irq(GPIO_T_FLASH_DETECT);
			if (ext_cd_irq &&
			    request_threaded_irq(ext_cd_irq, NULL,
						 handler,
						 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						 dev_name(dev), host) == 0) {
				dev_warn(dev, "success to request irq for card detect.\n");
				enable_irq_wake(ext_cd_irq);
			} else {
				dev_warn(dev, "cannot request irq for card detect.\n");
				ext_cd_irq = 0;
			}
			gpio_free(GPIO_T_FLASH_DETECT);
		} else {
			dev_err(dev, "cannot request gpio for card detect.\n");
		}
	}

	return 0;
}

static void exynos_dwmci2_exit(u32 slot_id)
{
	struct dw_mci *host = host2;

	if (ext_cd_irq)
		free_irq(ext_cd_irq, host);
}

static struct dw_mci_board universal5410_dwmci2_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED | DW_MMC_QUIRK_NO_VOLSW_INT,
	.bus_hz			= 80 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_UHS_SDR50,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci2_cfg_gpio,
	.get_ro			= universal5410_dwmci_get_ro,
	.get_bus_wd		= exynos_dwmci2_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.sdr_timing		= 0x03040000,
	.ddr_timing		= 0x03020000,
	.get_cd			= exynos_dwmci2_get_cd,
	.cd_type		= DW_MCI_CD_GPIO,
	.init			= exynos_dwmci2_init,
	.exit			= exynos_dwmci2_exit,
	.clk_drv		= 0x3,
	.clk_tbl		= exynos_dwmci_clk_rates_for_cpll,
	.__drv_st		= {
		.pin			= EXYNOS5410_GPC2(0),
		.val			= S5P_GPIO_DRVSTR_LV4,
	},
	.qos_int_level		= 160 * 1000,
};

static struct platform_device *universal5410_mmc_devices[] __initdata = {
#ifdef CONFIG_MMC_DW
	&exynos5_device_dwmci0,
	&exynos5_device_dwmci1,
	&exynos5_device_dwmci2,
#endif
};

void __init exynos5_universal5410_mmc_init(void)
{
#ifdef CONFIG_MMC_DW
	if (samsung_rev() < EXYNOS5410_REV_1_0)
		universal5410_dwmci0_pdata.caps &=
			~(MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR);

	exynos_dwmci_set_platdata(&universal5410_dwmci0_pdata, 0);
	exynos_dwmci_set_platdata(&universal5410_dwmci1_pdata, 1);
	exynos_dwmci_set_platdata(&universal5410_dwmci2_pdata, 2);
#endif
	platform_add_devices(universal5410_mmc_devices,
			ARRAY_SIZE(universal5410_mmc_devices));
}
