/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/c2c.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>

#include <mach/c2c.h>

void exynos_c2c_set_cprst(void)
{
	/* TODO */
}

void exynos_c2c_clear_cprst(void)
{
	/* TODO */
}

void EXYNOS5410_c2c_cfg_gpio(enum c2c_buswidth rx_width,
			enum c2c_buswidth tx_width, void __iomem *etc8drv_addr)
{
	int i;
	s5p_gpio_drvstr_t lv1 = S5P_GPIO_DRVSTR_LV1;
	s5p_gpio_drvstr_t lv3 = S5P_GPIO_DRVSTR_LV3;
	s5p_gpio_pd_cfg_t pd_cfg = S5P_GPIO_PD_INPUT;
	s5p_gpio_pd_pull_t pd_pull = S5P_GPIO_PD_DOWN_ENABLE;

	/* Set GPIO for C2C Rx */
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV0(0), 8, C2C_SFN);
	for (i = 0; i < 8; i++) {
		s5p_gpio_set_drvstr(EXYNOS5410_GPV0(i), lv1);
		s5p_gpio_set_pd_cfg(EXYNOS5410_GPV0(i), pd_cfg);
		s5p_gpio_set_pd_pull(EXYNOS5410_GPV0(i), pd_pull);
	}

	if (rx_width == C2C_BUSWIDTH_16) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV1(0), 8, C2C_SFN);
		for (i = 0; i < 8; i++) {
			s5p_gpio_set_drvstr(EXYNOS5410_GPV1(i), lv1);
			s5p_gpio_set_pd_cfg(EXYNOS5410_GPV1(i), pd_cfg);
			s5p_gpio_set_pd_pull(EXYNOS5410_GPV1(i), pd_pull);
		}
	} else if (rx_width == C2C_BUSWIDTH_10) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV1(0), 2, C2C_SFN);
		for (i = 0; i < 2; i++) {
			s5p_gpio_set_drvstr(EXYNOS5410_GPV1(i), lv1);
			s5p_gpio_set_pd_cfg(EXYNOS5410_GPV1(i), pd_cfg);
			s5p_gpio_set_pd_pull(EXYNOS5410_GPV1(i), pd_pull);
		}
	}

	/* Set GPIO for C2C Tx */
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV2(0), 8, C2C_SFN);
	for (i = 0; i < 8; i++)
		s5p_gpio_set_drvstr(EXYNOS5410_GPV2(i), lv3);

	if (tx_width == C2C_BUSWIDTH_16) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV3(0), 8, C2C_SFN);
		for (i = 0; i < 8; i++)
			s5p_gpio_set_drvstr(EXYNOS5410_GPV3(i), lv3);
	} else if (tx_width == C2C_BUSWIDTH_10) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV3(0), 2, C2C_SFN);
		for (i = 0; i < 2; i++)
			s5p_gpio_set_drvstr(EXYNOS5410_GPV3(i), lv3);
	}

	/* Set GPIO for WakeReqOut/In */
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV4(0), 2, C2C_SFN);
	s5p_gpio_set_pd_cfg(EXYNOS5410_GPV4(0), pd_cfg);
	s5p_gpio_set_pd_pull(EXYNOS5410_GPV4(0), pd_pull);

	writel(0x5, etc8drv_addr);
}

void exynos5410_c2c_cfg_gpio(enum c2c_buswidth rx_width,
			enum c2c_buswidth tx_width, void __iomem *etc8drv_addr)
{
	int i;
	s5p_gpio_drvstr_t lv1 = S5P_GPIO_DRVSTR_LV1;
	s5p_gpio_drvstr_t lv4 = S5P_GPIO_DRVSTR_LV4;
	s5p_gpio_pd_cfg_t pd_cfg = S5P_GPIO_PD_PREV_STATE;
	s5p_gpio_pd_pull_t pd_pull = S5P_GPIO_PD_DOWN_ENABLE;

	/* Set GPIO for C2C Rx */
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV0(0), 8, C2C_SFN);
	for (i = 0; i < 8; i++) {
		s5p_gpio_set_drvstr(EXYNOS5410_GPV0(i), lv1);
		s5p_gpio_set_pd_cfg(EXYNOS5410_GPV0(i), pd_cfg);
		s5p_gpio_set_pd_pull(EXYNOS5410_GPV0(i), pd_pull);
	}

	if (rx_width == C2C_BUSWIDTH_16) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV1(0), 8, C2C_SFN);
		for (i = 0; i < 8; i++) {
			s5p_gpio_set_drvstr(EXYNOS5410_GPV1(i), lv1);
			s5p_gpio_set_pd_cfg(EXYNOS5410_GPV1(i), pd_cfg);
			s5p_gpio_set_pd_pull(EXYNOS5410_GPV1(i), pd_pull);
		}
	} else if (rx_width == C2C_BUSWIDTH_10) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV1(0), 2, C2C_SFN);
		for (i = 0; i < 2; i++) {
			s5p_gpio_set_drvstr(EXYNOS5410_GPV1(i), lv1);
			s5p_gpio_set_pd_cfg(EXYNOS5410_GPV1(i), pd_cfg);
			s5p_gpio_set_pd_pull(EXYNOS5410_GPV1(i), pd_pull);
		}
	}

	/* Set GPIO for C2C Tx */
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV2(0), 8, C2C_SFN);
	for (i = 0; i < 8; i++) {
		s5p_gpio_set_drvstr(EXYNOS5410_GPV2(i), lv4);
		s5p_gpio_set_pd_cfg(EXYNOS5410_GPV2(i), pd_cfg);
	}

	if (tx_width == C2C_BUSWIDTH_16) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV3(0), 8, C2C_SFN);
		for (i = 0; i < 8; i++) {
			s5p_gpio_set_drvstr(EXYNOS5410_GPV3(i), lv4);
			s5p_gpio_set_pd_cfg(EXYNOS5410_GPV3(i), pd_cfg);
		}
	} else if (tx_width == C2C_BUSWIDTH_10) {
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV3(0), 2, C2C_SFN);
		for (i = 0; i < 2; i++) {
			s5p_gpio_set_drvstr(EXYNOS5410_GPV3(i), lv4);
			s5p_gpio_set_pd_cfg(EXYNOS5410_GPV3(i), pd_cfg);
		}
	}

	/* Set GPIO for WakeReqOut/In */
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPV4(0), 2, C2C_SFN);
	s5p_gpio_set_pd_cfg(EXYNOS5410_GPV4(0), pd_cfg);
	s5p_gpio_set_pd_pull(EXYNOS5410_GPV4(0), pd_pull);
	s5p_gpio_set_pd_cfg(EXYNOS5410_GPV4(1), pd_cfg);
	s5p_gpio_set_pd_pull(EXYNOS5410_GPV4(1), pd_pull);

	writel(0xf, etc8drv_addr);
}

void exynos_c2c_cfg_gpio(enum c2c_buswidth rx_width, enum c2c_buswidth tx_width)
{
	void __iomem *etc8drv_addr;

	etc8drv_addr = ioremap(EXYNOS5_PA_ETC8, SZ_64);
	etc8drv_addr += 0xC;

	if (soc_is_exynos5250()) {
		/* ETC8DRV is used for setting Tx clock drive strength */
		EXYNOS5410_c2c_cfg_gpio(rx_width, tx_width, etc8drv_addr);
	} else if (soc_is_exynos5410()) {
		/* ETC8DRV is used for setting Tx clock drive strength */
		exynos5410_c2c_cfg_gpio(rx_width, tx_width, etc8drv_addr);
	}

	iounmap(etc8drv_addr);
}
EXPORT_SYMBOL(exynos_c2c_cfg_gpio);

#ifdef CONFIG_C2C_IPC_ONLY
struct exynos_c2c_platdata smdk5410_c2c_pdata = {
	.setup_gpio = NULL,
	.shdmem_addr = 0,
	.shdmem_size = C2C_MEMSIZE_4,
	.ap_sscm_addr = NULL,
	.cp_sscm_addr = NULL,
	.rx_width = C2C_BUSWIDTH_8,
	.tx_width = C2C_BUSWIDTH_8,
	.clk_opp100 = 133,
	.clk_opp50 = 66,
	.clk_opp25 = 33,
	.default_opp_mode = C2C_OPP100,
	.get_c2c_state = NULL,
	.c2c_sysreg = NULL,
};
#else
struct exynos_c2c_platdata smdk5410_c2c_pdata = {
	.setup_gpio = NULL,
	.shdmem_addr = 0,
#ifndef CONFIG_C2C_USE_4MB
	.shdmem_size = C2C_MEMSIZE_64,
#else
	.shdmem_size = C2C_MEMSIZE_4,
#endif
	.ap_sscm_addr = NULL,
	.cp_sscm_addr = NULL,
	.rx_width = C2C_BUSWIDTH_8,
	.tx_width = C2C_BUSWIDTH_8,
	.clk_opp100 = 133,
	.clk_opp50 = 66,
	.clk_opp25 = 33,
	.default_opp_mode = C2C_OPP100,
	.get_c2c_state = NULL,
	.c2c_sysreg = NULL,
};
#endif

enum c2c_buswidth exynos_c2c_rx_width(void)
{
	return smdk5410_c2c_pdata.rx_width;
}
EXPORT_SYMBOL(exynos_c2c_rx_width);

enum c2c_buswidth exynos_c2c_tx_width(void)
{
	return smdk5410_c2c_pdata.tx_width;
}
EXPORT_SYMBOL(exynos_c2c_tx_width);

static struct platform_device *smdk5410_modem_if_devices[] __initdata = {
#ifdef CONFIG_EXYNOS_C2C
	&exynos_device_c2c,
#endif
};

void __init exynos5_universal5410_c2c_init(void)
{
#ifdef CONFIG_EXYNOS_C2C
	exynos_c2c_set_platdata(&smdk5410_c2c_pdata);
#endif
	platform_add_devices(smdk5410_modem_if_devices,
			ARRAY_SIZE(smdk5410_modem_if_devices));
}

