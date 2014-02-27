/*
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>

#include <asm/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>

#if defined(CONFIG_MFD_MAX77802)
#include <linux/mfd/max77802.h>
#endif

#include "board-universal5410.h"

#if defined(CONFIG_REGULATOR_MAX77802)
/* max77802 */

static struct regulator_consumer_supply max77802_buck1 =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply max77802_buck2 =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply max77802_buck3 =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply max77802_buck4 =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply max77802_buck6 =
	REGULATOR_SUPPLY("vdd_kfc", NULL);

static struct regulator_consumer_supply ldo2_supply[] = {
	REGULATOR_SUPPLY("vmem2_1.2v_ap", NULL),
};

#ifdef CONFIG_SND_SOC_WM8994
static struct regulator_consumer_supply ldo3_supply[] = {
	REGULATOR_SUPPLY("AVDD2", NULL),
	REGULATOR_SUPPLY("CPVDD", NULL),
	REGULATOR_SUPPLY("DBVDD1", NULL),
	REGULATOR_SUPPLY("DBVDD2", NULL),
	REGULATOR_SUPPLY("DBVDD3", NULL),
};
#else
static struct regulator_consumer_supply ldo3_supply[] = {};
#endif

static struct regulator_consumer_supply ldo4_supply[] = {
	REGULATOR_SUPPLY("vmmc2_2.8v_ap", NULL),
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2"),
};

static struct regulator_consumer_supply ldo5_supply[] = {
	REGULATOR_SUPPLY("vhsic_1.8v_ap", NULL),
};

static struct regulator_consumer_supply ldo6_supply[] = {
	REGULATOR_SUPPLY("vxpll_1.8v_ap", NULL),
};

static struct regulator_consumer_supply ldo7_supply[] = {
	REGULATOR_SUPPLY("vcc_1.9v_irda", NULL),
};

static struct regulator_consumer_supply ldo8_supply[] = {
	REGULATOR_SUPPLY("vmipi_1.0v_ap", NULL),
};

static struct regulator_consumer_supply ldo9_supply[] = {
	REGULATOR_SUPPLY("vgesture_1.8v", NULL),
};

static struct regulator_consumer_supply ldo10_supply[] = {
	REGULATOR_SUPPLY("vmipi_1.8v_ap", NULL),
};

static struct regulator_consumer_supply ldo11_supply[] = {
	REGULATOR_SUPPLY("vddq_pre_1.8v", NULL),
};

static struct regulator_consumer_supply ldo12_supply[] = {
	REGULATOR_SUPPLY("vuotg_3.0v_ap", NULL),
};

static struct regulator_consumer_supply ldo14_supply[] = {
	REGULATOR_SUPPLY("vabb1_1.8v_ap", NULL),
};

static struct regulator_consumer_supply ldo15_supply[] = {
	REGULATOR_SUPPLY("vhsic_1.0v_ap", NULL),
};

static struct regulator_consumer_supply ldo18_supply[] = {
	REGULATOR_SUPPLY("cam_io_1.8v", NULL),
};

static struct regulator_consumer_supply ldo19_supply[] = {
	REGULATOR_SUPPLY("2m_cam_1.8v", NULL),
};

static struct regulator_consumer_supply ldo23_supply[] = {
	REGULATOR_SUPPLY("touch_avdd_2.8v", NULL),
	REGULATOR_SUPPLY("tsp_avdd", NULL),
};

static struct regulator_consumer_supply ldo24_supply[] = {
	REGULATOR_SUPPLY("5m_core_1.5v", NULL),
};

static struct regulator_consumer_supply ldo25_supply[] = {
	REGULATOR_SUPPLY("vcc_3.3v_mhl", NULL),
};

static struct regulator_consumer_supply ldo26_supply[] = {
	REGULATOR_SUPPLY("key_led_3.3v", NULL),
};

static struct regulator_consumer_supply ldo27_supply[] = {
	REGULATOR_SUPPLY("vsil_1.2a", NULL),
};

static struct regulator_consumer_supply ldo28_supply[] = {
	REGULATOR_SUPPLY("vcc_1.8v_mhl", NULL),
};

static struct regulator_consumer_supply ldo29_supply[] = {
	REGULATOR_SUPPLY("tsp_vdd_1.8v", NULL),
	REGULATOR_SUPPLY("touch_1.8v", NULL),
};

static struct regulator_consumer_supply ldo30_supply[] = {
	REGULATOR_SUPPLY("vmem1_1.2v_ap", NULL),
};

static struct regulator_consumer_supply ldo32_supply[] = {
	REGULATOR_SUPPLY("vadc_3.3v", NULL),
};

static struct regulator_consumer_supply ldo33_supply[] = {
	REGULATOR_SUPPLY("vcc_2.8v_ap", NULL),
};

#if 0
static struct regulator_consumer_supply max77802_enp32khz[] = {
	REGULATOR_SUPPLY("lpo_in", "bcm47511"),
	REGULATOR_SUPPLY("lpo", "bcm4334_bluetooth"),
};
#endif

#define REGULATOR_INIT(_ldo, _name, _min_uV, _max_uV, _always_on, _ops_mask, \
		       _disabled)					\
	static struct regulator_init_data _ldo##_init_data = {		\
		.constraints = {					\
			.name = _name,					\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on	= _always_on,			\
			.boot_on	= _always_on,			\
			.apply_uV	= 1,				\
			.valid_ops_mask = _ops_mask,			\
			.state_mem = {					\
				.disabled	= _disabled,		\
				.enabled	= !(_disabled),		\
			}						\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(_ldo##_supply),	\
		.consumer_supplies = &_ldo##_supply[0],			\
	};

REGULATOR_INIT(ldo2, "vmem2_1.2v_ap", 1200000, 1200000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo3, "vcc_1.8v_ap", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo4, "vmmc2_2.8v_ap", 1800000, 2800000, 0,
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo5, "vhsic_1.8v_ap", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo6, "vxpll_1.8v_ap", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo7, "vcc_1.9v_irda", 1900000, 1900000, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo8, "vmipi_1.0v_ap", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo9, "vgesture_1.8v", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo10, "vmipi_1.8v_ap", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo11, "vddq_pre_1.8v", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo12, "vuotg_3.0v_ap", 3000000, 3000000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo14, "vabb1_1.8v_ap", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo15, "vhsic_1.0v_ap", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo18, "cam_io_1.8v", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo19, "2m_cam_1.8v", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo23, "touch_avdd_2.8v", 2800000, 2800000, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo24, "5m_core_1.5v", 1500000, 1500000, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo25, "vcc_3.3v_mhl", 3300000, 3300000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo26, "key_led_3.3v", 3300000, 3300000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo27, "vsil_1.2a", 1200000, 1200000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo28, "vcc_1.8v_mhl", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo29, "tsp_vdd_1.8v", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo30, "vmem1_1.2v_ap", 1200000, 1200000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo32, "vadc_3.3V", 3300000, 3300000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo33, "vcc_2.8v_ap", 2800000, 2800000, 1,
		REGULATOR_CHANGE_STATUS, 0);


static struct regulator_init_data max77802_buck1_data = {
	.constraints = {
		.name = "vdd_mif range",
		.min_uV = 800000,
		.max_uV = 1300000,
		.always_on = 1,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77802_buck1,
};

static struct regulator_init_data max77802_buck2_data = {
	.constraints = {
		.name = "vdd_arm range",
		.min_uV = 800000,
		.max_uV = 1500000,
		.apply_uV = 1,
		.always_on = 1,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77802_buck2,
};

static struct regulator_init_data max77802_buck3_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		=  800000,
		.max_uV		= 1400000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77802_buck3,
};

static struct regulator_init_data max77802_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		=  800000,
		.max_uV		= 1400000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77802_buck4,
};

static struct regulator_init_data max77802_buck6_data = {
	.constraints	= {
		.name		= "vdd_kfc range",
		.min_uV		=  800000,
		.max_uV		= 1500000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77802_buck6,
};
#if 0
static struct regulator_init_data max77802_enp32khz_data = {
	.constraints = {
		.name = "32KHZ_PMIC",
		.always_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= 1,
			.disabled	= 0,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(max77802_enp32khz),
	.consumer_supplies = max77802_enp32khz,
};
#endif

static struct max77802_regulator_data max77802_regulators[] = {
	{MAX77802_BUCK1, &max77802_buck1_data,},
	{MAX77802_BUCK2, &max77802_buck2_data,},
	{MAX77802_BUCK3, &max77802_buck3_data,},
	{MAX77802_BUCK4, &max77802_buck4_data,},
	{MAX77802_BUCK6, &max77802_buck6_data,},
	{MAX77802_LDO2, &ldo2_init_data,},
	{MAX77802_LDO3, &ldo3_init_data,},
	{MAX77802_LDO4, &ldo4_init_data,},
	{MAX77802_LDO5, &ldo5_init_data,},
	{MAX77802_LDO6, &ldo6_init_data,},
	{MAX77802_LDO7, &ldo7_init_data,},
	{MAX77802_LDO8, &ldo8_init_data,},
	{MAX77802_LDO9, &ldo9_init_data,},
	{MAX77802_LDO10, &ldo10_init_data,},
	{MAX77802_LDO11, &ldo11_init_data,},
	{MAX77802_LDO12, &ldo12_init_data,},
	{MAX77802_LDO14, &ldo14_init_data,},
	{MAX77802_LDO15, &ldo15_init_data,},
	{MAX77802_LDO18, &ldo18_init_data,},
	{MAX77802_LDO19, &ldo19_init_data,},
	{MAX77802_LDO23, &ldo23_init_data,},
	{MAX77802_LDO24, &ldo24_init_data,},
	{MAX77802_LDO25, &ldo25_init_data,},
	{MAX77802_LDO26, &ldo26_init_data,},
	{MAX77802_LDO27, &ldo27_init_data,},
	{MAX77802_LDO28, &ldo28_init_data,},
	{MAX77802_LDO29, &ldo29_init_data,},
	{MAX77802_LDO30, &ldo30_init_data,},
	{MAX77802_LDO32, &ldo32_init_data,},
	{MAX77802_LDO33, &ldo33_init_data,},
#if 0
	{MAX77802_P32KH, &max77802_enp32khz_data,},
#endif
};

struct max77802_opmode_data max77802_opmode_data[MAX77802_REG_MAX] = {
	[MAX77802_LDO2] = {MAX77802_LDO2, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO4] = {MAX77802_LDO4, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO5] = {MAX77802_LDO5, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO6] = {MAX77802_LDO6, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO8] = {MAX77802_LDO8, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO10] = {MAX77802_LDO10, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO11] = {MAX77802_LDO11, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO12] = {MAX77802_LDO12, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO14] = {MAX77802_LDO14, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO15] = {MAX77802_LDO15, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO23] = {MAX77802_LDO23, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO29] = {MAX77802_LDO29, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO30] = {MAX77802_LDO30, MAX77802_OPMODE_STANDBY},
	[MAX77802_LDO32] = {MAX77802_LDO32, MAX77802_OPMODE_STANDBY},
	[MAX77802_BUCK1] = {MAX77802_BUCK1, MAX77802_OPMODE_STANDBY},
	[MAX77802_BUCK2] = {MAX77802_BUCK2, MAX77802_OPMODE_STANDBY},
	[MAX77802_BUCK3] = {MAX77802_BUCK3, MAX77802_OPMODE_STANDBY},
	[MAX77802_BUCK4] = {MAX77802_BUCK4, MAX77802_OPMODE_STANDBY},
	[MAX77802_BUCK6] = {MAX77802_BUCK6, MAX77802_OPMODE_STANDBY},
};

struct max77802_platform_data exynos4_max77802_info = {
	.num_regulators = ARRAY_SIZE(max77802_regulators),
	.regulators = max77802_regulators,
	.irq_gpio	= GPIO_AP_PMIC_IRQ,
	.irq_base	= IRQ_BOARD_START,
	.wakeup		= 1,

	.opmode_data = max77802_opmode_data,
	.ramp_rate = MAX77802_RAMP_RATE_25MV,
	.wtsr_smpl = MAX77802_WTSR_ENABLE | MAX77802_SMPL_ENABLE,

	.buck12346_gpio_dvs = {
		/* Use DVS2 register of each bucks to supply stable power
		 * after sudden reset */
		{GPIO_PMIC_DVS1, 1},
		{GPIO_PMIC_DVS2, 0},
		{GPIO_PMIC_DVS3, 0},
	},
	.buck12346_gpio_selb = {
		GPIO_BUCK1_SEL,
		GPIO_BUCK2_SEL,
		GPIO_BUCK3_SEL,
		GPIO_BUCK4_SEL,
		GPIO_BUCK6_SEL,
	},
	.buck1_voltage[0] = 1100000,	/* 1.1V */
	.buck1_voltage[1] = 1100000,	/* 1.1V */
	.buck1_voltage[2] = 1100000,	/* 1.1V */
	.buck1_voltage[3] = 1100000,	/* 1.1V */
	.buck1_voltage[4] = 1100000,	/* 1.1V */
	.buck1_voltage[5] = 1100000,	/* 1.1V */
	.buck1_voltage[6] = 1100000,	/* 1.1V */
	.buck1_voltage[7] = 1100000,	/* 1.1V */

	.buck2_voltage[0] = 1100000,	/* 1.1V */
	.buck2_voltage[1] = 1100000,	/* 1.1V */
	.buck2_voltage[2] = 1100000,	/* 1.1V */
	.buck2_voltage[3] = 1100000,	/* 1.1V */
	.buck2_voltage[4] = 1100000,	/* 1.1V */
	.buck2_voltage[5] = 1100000,	/* 1.1V */
	.buck2_voltage[6] = 1100000,	/* 1.1V */
	.buck2_voltage[7] = 1100000,	/* 1.1V */

	.buck3_voltage[0] = 1100000,	/* 1.1V */
	.buck3_voltage[1] = 1000000,	/* 1.0V */
	.buck3_voltage[2] = 1100000,	/* 1.1V */
	.buck3_voltage[3] = 1100000,	/* 1.1V */
	.buck3_voltage[4] = 1100000,	/* 1.1V */
	.buck3_voltage[5] = 1100000,	/* 1.1V */
	.buck3_voltage[6] = 1100000,	/* 1.1V */
	.buck3_voltage[7] = 1100000,	/* 1.1V */

	.buck4_voltage[0] = 1100000,	/* 1.1V */
	.buck4_voltage[1] = 1000000,	/* 1.0V */
	.buck4_voltage[2] = 1100000,	/* 1.1V */
	.buck4_voltage[3] = 1100000,	/* 1.1V */
	.buck4_voltage[4] = 1100000,	/* 1.1V */
	.buck4_voltage[5] = 1100000,	/* 1.1V */
	.buck4_voltage[6] = 1100000,	/* 1.1V */
	.buck4_voltage[7] = 1100000,	/* 1.1V */

	.buck6_voltage[0] = 1100000,	/* 1.1V */
	.buck6_voltage[1] = 1000000,	/* 1.0V */
	.buck6_voltage[2] = 1100000,	/* 1.1V */
	.buck6_voltage[3] = 1100000,	/* 1.1V */
	.buck6_voltage[4] = 1100000,	/* 1.1V */
	.buck6_voltage[5] = 1100000,	/* 1.1V */
	.buck6_voltage[6] = 1100000,	/* 1.1V */
	.buck6_voltage[7] = 1100000,	/* 1.1V */
};

struct i2c_board_info hs_i2c_devs0[PMIC_I2C_DEVS_MAX] __initdata = {
	{
		I2C_BOARD_INFO("max77802", 0x12 >> 1),
		.platform_data = &exynos4_max77802_info,
	},
};
#endif /* CONFIG_REGULATOR_MAX77802 */
