/*
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
#include <linux/gpio.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include <mach/irqs.h>
#include <mach/hs-iic.h>

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps11.h>

#define SMDK5420_PMIC_EINT	IRQ_EINT(7)

static struct regulator_consumer_supply s2m_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s2m_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s2m_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s2m_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply s2m_buck6_consumer =
	REGULATOR_SUPPLY("vdd_kfc", NULL);

static struct regulator_consumer_supply s2m_ldo13_consumer[] = {
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2"),
};

static struct regulator_consumer_supply s2m_ldo14_consumer[] = {
	REGULATOR_SUPPLY("vcc_3.0v_motor", NULL),
};

static struct regulator_consumer_supply s2m_ldo15_consumer[] = {
	REGULATOR_SUPPLY("wacom_3.0v", NULL),
};

static struct regulator_consumer_supply s2m_ldo17_consumer[] = {
	REGULATOR_SUPPLY("tsp_avdd", NULL),
};

static struct regulator_consumer_supply s2m_ldo20_consumer[] = {
        REGULATOR_SUPPLY("vt_cam_1.8v", NULL),
};

static struct regulator_consumer_supply s2m_ldo21_consumer[] = {
        REGULATOR_SUPPLY("cam_isp_sensor_io_1.8v", NULL),
};

static struct regulator_consumer_supply s2m_ldo22_consumer[] = {
        REGULATOR_SUPPLY("cam_sensor_core_1.2v", NULL),
};

static struct regulator_consumer_supply s2m_ldo23_consumer[] = {
	REGULATOR_SUPPLY("vdd_mifs", NULL),
};

static struct regulator_consumer_supply s2m_ldo24_consumer[] = {
        REGULATOR_SUPPLY("cam_sensor_a2.8v_pm", NULL),
};

static struct regulator_consumer_supply s2m_ldo25_consumer[] = {
	REGULATOR_SUPPLY("vcc_1.8v_lcd", NULL),
};

static struct regulator_consumer_supply s2m_ldo26_consumer[] = {
        REGULATOR_SUPPLY("cam_af_2.8v_pm", NULL),
};

static struct regulator_consumer_supply s2m_ldo27_consumer[] = {
	REGULATOR_SUPPLY("vdd_g3ds", NULL),
};

static struct regulator_consumer_supply s2m_ldo28_consumer[] = {
	REGULATOR_SUPPLY("vcc_3.0v_lcd", NULL),
};

static struct regulator_consumer_supply s2m_ldo30_consumer[] = {
	REGULATOR_SUPPLY("vtouch_1.8v", NULL),
};

static struct regulator_consumer_supply s2m_ldo32_consumer[] = {
	REGULATOR_SUPPLY("touch_1.8v", NULL),
};

static struct regulator_consumer_supply s2m_ldo33_consumer[] = {
	REGULATOR_SUPPLY("vcc_1.8v_mhl", NULL),
};

static struct regulator_consumer_supply s2m_ldo34_consumer[] = {
	REGULATOR_SUPPLY("vcc_3.3v_mhl", NULL),
};

static struct regulator_consumer_supply s2m_ldo35_consumer[] = {
	REGULATOR_SUPPLY("vsil_1.2a", NULL),
};

static struct regulator_consumer_supply s2m_ldo38_consumer[] = {
	REGULATOR_SUPPLY("vtouch_3.3v", NULL),
};

static struct regulator_init_data s2m_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		=  700000,
		.max_uV		= 1300000,
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
	.consumer_supplies	= &s2m_buck1_consumer,
};

static struct regulator_init_data s2m_buck2_data = {
	.constraints	= {
		.name		= "vdd_arm range",
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
	.consumer_supplies	= &s2m_buck2_consumer,
};

static struct regulator_init_data s2m_buck3_data = {
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
	.consumer_supplies	= &s2m_buck3_consumer,
};

static struct regulator_init_data s2m_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		=  700000,
		.max_uV		= 1400000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck4_consumer,
};

static struct regulator_init_data s2m_buck6_data = {
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
	.consumer_supplies	= &s2m_buck6_consumer,
};

#define REGULATOR_INIT(_ldo, _name, _min_uV, _max_uV, _always_on,	\
		_boot_on, _ops_mask, _enabled)				\
	static struct regulator_init_data s2m_##_ldo##_data = {		\
		.constraints = {					\
			.name	= _name,				\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on	= _always_on,			\
			.boot_on	= _boot_on,			\
			.apply_uV	= 1,				\
			.valid_ops_mask = _ops_mask,			\
			.state_mem	= {				\
				.disabled =				\
					(_enabled == -1 ? 0 : !(_enabled)),\
				.enabled =				\
					(_enabled == -1 ? 0 : _enabled),\
			},						\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(s2m_##_ldo##_consumer),\
		.consumer_supplies = &s2m_##_ldo##_consumer[0],		\
	};

REGULATOR_INIT(ldo13, "VMMC2_2.8V_AP", 1800000, 2800000, 0, 1,
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo14, "VCC_3.0V_MOTOR", 3000000, 3000000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo15, "WACOM_3.3V", 3300000, 3300000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo17, "TSP_AVDD_3.3V", 3300000, 3300000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo20, "VT_CAM_1.8V", 1800000, 1800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo21, "CAM_ISP_SENSOR_IO_1.8V", 1800000, 1800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo22, "CAM_ISP_SENSOR_CORE_1.05V_PM", 1050000, 1200000, 0, 0,
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo23, "vdd_mifs range", 800000, 1100000, 1, 0,
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo24, "CAM_SENSOR_A2.8V_PM", 2800000, 2800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo25, "VCC_1.8V_LCD", 1800000, 1800000, 0, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo26, "CAM_AF_2.8V_PM", 2800000, 2800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo27, "vdd_g3ds range", 800000, 1100000, 1, 0,
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo28, "VCC_3.0V_LCD", 3000000, 3000000, 0, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo30, "VTOUCH_1.8V", 1800000, 1800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo32, "TSP_VDD_1.8V", 1800000, 1800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo33, "VCC_1.8V_MHL", 1800000, 1800000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo34, "VCC_3.3V_MHL", 3300000, 3300000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo35, "VSIL_1.2A", 1200000, 1200000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo38, "VTOUCH_3.3V", 3300000, 3300000, 0, 0,
		REGULATOR_CHANGE_STATUS, 0);

static struct sec_regulator_data exynos_regulators[] = {
	{S2MPS11_BUCK1, &s2m_buck1_data},
	{S2MPS11_BUCK2, &s2m_buck2_data},
	{S2MPS11_BUCK3, &s2m_buck3_data},
	{S2MPS11_BUCK4, &s2m_buck4_data},
	{S2MPS11_BUCK6, &s2m_buck6_data},
	{S2MPS11_LDO13, &s2m_ldo13_data},
	{S2MPS11_LDO14, &s2m_ldo14_data},
	{S2MPS11_LDO15, &s2m_ldo15_data},
	{S2MPS11_LDO17, &s2m_ldo17_data},
	{S2MPS11_LDO20, &s2m_ldo20_data},
        {S2MPS11_LDO21, &s2m_ldo21_data},
        {S2MPS11_LDO22, &s2m_ldo22_data},
	{S2MPS11_LDO23, &s2m_ldo23_data},
        {S2MPS11_LDO24, &s2m_ldo24_data},
	{S2MPS11_LDO25, &s2m_ldo25_data},
	{S2MPS11_LDO26, &s2m_ldo26_data},
	{S2MPS11_LDO27, &s2m_ldo27_data},
	{S2MPS11_LDO28, &s2m_ldo28_data},
	{S2MPS11_LDO30, &s2m_ldo30_data},
	{S2MPS11_LDO32, &s2m_ldo32_data},
	{S2MPS11_LDO33, &s2m_ldo33_data},
	{S2MPS11_LDO34, &s2m_ldo34_data},
	{S2MPS11_LDO35, &s2m_ldo35_data},
	{S2MPS11_LDO38, &s2m_ldo38_data},
};

struct sec_opmode_data s2mps11_opmode_data[S2MPS11_REG_MAX] = {
	[S2MPS11_BUCK1] = {S2MPS11_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK2] = {S2MPS11_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK3] = {S2MPS11_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK4] = {S2MPS11_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK6] = {S2MPS11_BUCK6, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO13] = {S2MPS11_LDO13, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO14] = {S2MPS11_LDO14, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO15] = {S2MPS11_LDO15, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO17] = {S2MPS11_LDO17, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO20] = {S2MPS11_LDO20, SEC_OPMODE_STANDBY},
        [S2MPS11_LDO21] = {S2MPS11_LDO21, SEC_OPMODE_STANDBY},
        [S2MPS11_LDO22] = {S2MPS11_LDO22, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO23] = {S2MPS11_LDO23, SEC_OPMODE_STANDBY},
        [S2MPS11_LDO24] = {S2MPS11_LDO24, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO25] = {S2MPS11_LDO25, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO27] = {S2MPS11_LDO27, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO28] = {S2MPS11_LDO28, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO30] = {S2MPS11_LDO30, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO32] = {S2MPS11_LDO32, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO33] = {S2MPS11_LDO33, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO34] = {S2MPS11_LDO34, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO35] = {S2MPS11_LDO35, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO38] = {S2MPS11_LDO38, SEC_OPMODE_STANDBY},
};

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(SMDK5420_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);

	return 0;
}

static struct sec_pmic_platform_data exynos5_s2m_pdata = {
	.device_type		= S2MPS11X,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(exynos_regulators),
	.regulators		= exynos_regulators,
	.cfg_pmic_irq		= sec_cfg_irq,
	.wakeup			= 1,
	.wtsr_smpl		= 1,
	.opmode_data		= s2mps11_opmode_data,
	.buck16_ramp_delay	= 12,
	.buck2_ramp_delay	= 12,
	.buck34_ramp_delay	= 12,
	.buck2_ramp_enable	= 1,
	.buck3_ramp_enable	= 1,
	.buck4_ramp_enable	= 1,
	.buck6_ramp_enable	= 1,
};

static struct i2c_board_info hs_i2c_devs3[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data	= &exynos5_s2m_pdata,
		.irq		= SMDK5420_PMIC_EINT,
	},
};

struct exynos5_platform_i2c hs_i2c3_data __initdata = {
	.bus_number = 7,
	.operation_mode = HSI2C_POLLING,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

void __init board_h_pmic_init(void)
{
	exynos5_hs_i2c3_set_platdata(&hs_i2c3_data);
	i2c_register_board_info(7, hs_i2c_devs3, ARRAY_SIZE(hs_i2c_devs3));
	platform_device_register(&exynos5_device_hs_i2c3);
}
