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

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps11.h>

#include "board-universal5410.h"

#if defined(CONFIG_REGULATOR_S2MPS11)
/* S2MPS11 Regulator */
static struct regulator_consumer_supply s2m_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s2m_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s2m_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s2m_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply s2m_buck5v123_consumer =
	REGULATOR_SUPPLY("vdd_mem2", NULL);

static struct regulator_consumer_supply s2m_buck6_consumer =
	REGULATOR_SUPPLY("vdd_kfc", NULL);

#ifdef CONFIG_SND_SOC_WM8994
static struct regulator_consumer_supply s2m_ldo2_consumer[] = {
	REGULATOR_SUPPLY("AVDD2", NULL),
	REGULATOR_SUPPLY("CPVDD", NULL),
	REGULATOR_SUPPLY("DBVDD1", NULL),
	REGULATOR_SUPPLY("DBVDD2", NULL),
	REGULATOR_SUPPLY("DBVDD3", NULL),
};
#elif defined(CONFIG_MFD_WM5102)
static struct regulator_consumer_supply s2m_ldo2_consumer[] = {
	REGULATOR_SUPPLY("AVDD", "spi2.0"),
	REGULATOR_SUPPLY("LDOVDD", "spi2.0"),
	REGULATOR_SUPPLY("DBVDD1", "spi2.0"),

	REGULATOR_SUPPLY("AVDD", "12-001a"),
	REGULATOR_SUPPLY("LDOVDD", "12-001a"),
	REGULATOR_SUPPLY("DBVDD1", "12-001a"),

	REGULATOR_SUPPLY("CPVDD", "wm5102-codec"),
	REGULATOR_SUPPLY("DBVDD2", "wm5102-codec"),
	REGULATOR_SUPPLY("DBVDD3", "wm5102-codec"),
	REGULATOR_SUPPLY("SPKVDDL", "wm5102-codec"),
	REGULATOR_SUPPLY("SPKVDDR", "wm5102-codec"),
};
#else
static struct regulator_consumer_supply s2m_ldo2_consumer[] = {};
#endif

static struct regulator_consumer_supply s2m_ldo5_consumer =
	REGULATOR_SUPPLY("vxpll_1.8v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo6_consumer =
	REGULATOR_SUPPLY("vmipi_1.0v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo7_consumer =
	REGULATOR_SUPPLY("vmipi_1.8v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo8_consumer =
	REGULATOR_SUPPLY("vabb1_1.8v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo9_consumer =
	REGULATOR_SUPPLY("vuotg_3.0v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo10_consumer =
	REGULATOR_SUPPLY("vdd_ldo10", NULL);

static struct regulator_consumer_supply s2m_ldo11_consumer =
	REGULATOR_SUPPLY("vhsic_1.0v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo12_consumer =
	REGULATOR_SUPPLY("vdd_ldo12", NULL);

static struct regulator_consumer_supply s2m_ldo13_consumer =
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2");

static struct regulator_consumer_supply s2m_ldo14_consumer =
	REGULATOR_SUPPLY("vcc_3.0v_motor", NULL);

static struct regulator_consumer_supply s2m_ldo15_consumer =
	REGULATOR_SUPPLY("tsp_vdd_3.0v", NULL);

static struct regulator_consumer_supply s2m_ldo16_consumer =
	REGULATOR_SUPPLY("vcc_2.8v_ap", NULL);

static struct regulator_consumer_supply s2m_ldo17_consumer =
	REGULATOR_SUPPLY("tsp_avdd", NULL);

static struct regulator_consumer_supply s2m_ldo20_consumer =
	REGULATOR_SUPPLY("vt_cam_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo21_consumer =
	REGULATOR_SUPPLY("cam_isp_sensor_io_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo22_consumer =
	REGULATOR_SUPPLY("cam_sensor_core_1.2v", NULL);

static struct regulator_consumer_supply s2m_ldo23_consumer =
	REGULATOR_SUPPLY("v2mic_1.1v", NULL);

static struct regulator_consumer_supply s2m_ldo24_consumer =
	REGULATOR_SUPPLY("cam_sensor_a2.8v_pm", NULL);

static struct regulator_consumer_supply s2m_ldo25_consumer[] = {
	REGULATOR_SUPPLY("touch_1.8v_s", NULL),
	REGULATOR_SUPPLY("vcc_1.8v_lcd", NULL),
};

static struct regulator_consumer_supply s2m_ldo26_consumer =
	REGULATOR_SUPPLY("cam_af_2.8v_pm", NULL);

static struct regulator_consumer_supply s2m_ldo28_consumer =
	REGULATOR_SUPPLY("vcc_2.8v_lcd", NULL);

static struct regulator_consumer_supply s2m_ldo29_consumer =
	REGULATOR_SUPPLY("vgesture_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo30_consumer =
	REGULATOR_SUPPLY("vtouch_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo32_consumer =
	REGULATOR_SUPPLY("touch_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo33_consumer =
	REGULATOR_SUPPLY("vcc_1.8v_mhl", NULL);

static struct regulator_consumer_supply s2m_ldo34_consumer =
	REGULATOR_SUPPLY("vcc_3.3v_mhl", NULL);

static struct regulator_consumer_supply s2m_ldo35_consumer =
	REGULATOR_SUPPLY("vsil_1.2a", NULL);

static struct regulator_consumer_supply s2m_ldo37_consumer =
	REGULATOR_SUPPLY("uv_vcc_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo38_consumer[] = {
	REGULATOR_SUPPLY("tsp_avdd_s", NULL),
	REGULATOR_SUPPLY("vtouch_3.3v", NULL),
};

static struct regulator_init_data s2m_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		=  800000,
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
	.consumer_supplies	= &s2m_buck4_consumer,
};

static struct regulator_init_data s2m_buck5v123_data = {
	.constraints	= {
		.name		= "vdd_mem2 range",
		.min_uV		= 800000,
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
	.consumer_supplies	= &s2m_buck5v123_consumer,
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

static struct regulator_init_data s2m_ldo2_data = {
	.constraints    = {
		.name           = "vcc_1.8v_ap range",
		.min_uV         = 1800000,
		.max_uV         = 1800000,
		.apply_uV       = 1,
		.always_on      = 1,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = ARRAY_SIZE(s2m_ldo2_consumer),
	.consumer_supplies      = s2m_ldo2_consumer,
};

static struct regulator_init_data s2m_ldo5_data = {
	.constraints	= {
		.name		= "vxpll_1.8v_ap range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo5_consumer,
};

static struct regulator_init_data s2m_ldo6_data = {
	.constraints	= {
		.name		= "vmipi_1.0v_ap range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo6_consumer,
};

static struct regulator_init_data s2m_ldo7_data = {
	.constraints	= {
		.name		= "vmipi_1.8v_ap range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo7_consumer,
};

static struct regulator_init_data s2m_ldo8_data = {
	.constraints	= {
		.name		= "vabb1_1.8v_ap range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo8_consumer,
};

static struct regulator_init_data s2m_ldo9_data = {
	.constraints	= {
		.name		= "vuotg_3.0v_ap range",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo9_consumer,
};

static struct regulator_init_data s2m_ldo10_data = {
	.constraints	= {
		.name		= "vdd_ldo10 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo10_consumer,
};

static struct regulator_init_data s2m_ldo11_data = {
	.constraints	= {
		.name		= "vhsic_1.0v_ap range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo11_consumer,
};

static struct regulator_init_data s2m_ldo12_data = {
	.constraints	= {
		.name		= "vdd_ldo12 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo12_consumer,
};

static struct regulator_init_data s2m_ldo13_data = {
	.constraints	= {
		.name		= "vdd_ldo13 range",
		.min_uV		= 1800000,
		.max_uV		= 2800000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo13_consumer,
};

static struct regulator_init_data s2m_ldo14_data = {
	.constraints	= {
		.name		= "vcc_3.0v_motor range",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo14_consumer,
};

static struct regulator_init_data s2m_ldo15_data = {
	.constraints	= {
		.name		= "vcc_3.0v_lcd range",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo15_consumer,
};

static struct regulator_init_data s2m_ldo16_data = {
	.constraints	= {
		.name		= "vcc_2.8v_ap range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo16_consumer,
};

static struct regulator_init_data s2m_ldo17_data = {
	.constraints	= {
		.name		= "tsp_avdd range",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo17_consumer,
};

static struct regulator_init_data s2m_ldo20_data = {
	.constraints	= {
		.name		= "vt_cam_1.8v range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo20_consumer,
};

static struct regulator_init_data s2m_ldo21_data = {
	.constraints	= {
		.name		= "cam_isp_sensor_io_1.8v range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo21_consumer,
};

static struct regulator_init_data s2m_ldo22_data = {
	.constraints	= {
		.name		= "cam_sensor_core_1.2v range",
		.min_uV		= 1050000,
		.max_uV		= 1050000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo22_consumer,
};

static struct regulator_init_data s2m_ldo23_data = {
	.constraints	= {
		.name		= "v2mic_1.1v range",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo23_consumer,
};

static struct regulator_init_data s2m_ldo24_data = {
	.constraints	= {
		.name		= "cam_sensor_a2.8v_pm range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo24_consumer,
};

static struct regulator_init_data s2m_ldo25_data = {
	.constraints	= {
		.name		= "touch_1.8v_s range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(s2m_ldo25_consumer),
	.consumer_supplies	= s2m_ldo25_consumer,
};

static struct regulator_init_data s2m_ldo26_data = {
	.constraints	= {
		.name		= "cam_af_2.8v_pm range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo26_consumer,
};

static struct regulator_init_data s2m_ldo28_data = {
	.constraints	= {
		.name		= "vcc_2.8v_lcd",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo28_consumer,
};

static struct regulator_init_data s2m_ldo29_data = {
	.constraints	= {
		.name		= "gesture_sensor",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
			.disabled	= 0,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo29_consumer,
};

static struct regulator_init_data s2m_ldo30_data = {
	.constraints	= {
		.name		= "touchkey",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo30_consumer,
};

static struct regulator_init_data s2m_ldo32_data = {
	.constraints	= {
		.name		= "tsp_vdd range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo32_consumer,
};

static struct regulator_init_data s2m_ldo33_data = {
	.constraints	= {
		.name		= "vcc_1.8v_mhl",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo33_consumer,
};

static struct regulator_init_data s2m_ldo34_data = {
	.constraints	= {
		.name		= "vcc_3.3v_mhl",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo34_consumer,
};

static struct regulator_init_data s2m_ldo35_data = {
	.constraints	= {
		.name		= "vsil_1.2a",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo35_consumer,
};

static struct regulator_init_data s2m_ldo37_data = {
	.constraints	= {
		.name		= "uv_vcc_1.8v",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo37_consumer,
};

static struct regulator_init_data s2m_ldo38_data = {
	.constraints	= {
		.name		= "touchkey_led",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 0,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(s2m_ldo38_consumer),
	.consumer_supplies	= s2m_ldo38_consumer,
};

static struct sec_regulator_data exynos_regulators[] = {
	{S2MPS11_BUCK1, &s2m_buck1_data},
	{S2MPS11_BUCK2, &s2m_buck2_data},
	{S2MPS11_BUCK3, &s2m_buck3_data},
	{S2MPS11_BUCK4, &s2m_buck4_data},
	{S2MPS11_BUCK5V123, &s2m_buck5v123_data},
	{S2MPS11_BUCK6, &s2m_buck6_data},
	{S2MPS11_LDO2, &s2m_ldo2_data},
	{S2MPS11_LDO5, &s2m_ldo5_data},
	{S2MPS11_LDO6, &s2m_ldo6_data},
	{S2MPS11_LDO7, &s2m_ldo7_data},
	{S2MPS11_LDO8, &s2m_ldo8_data},
	{S2MPS11_LDO9, &s2m_ldo9_data},
	{S2MPS11_LDO10, &s2m_ldo10_data},
	{S2MPS11_LDO11, &s2m_ldo11_data},
	{S2MPS11_LDO12, &s2m_ldo12_data},
	{S2MPS11_LDO13, &s2m_ldo13_data},
	{S2MPS11_LDO14, &s2m_ldo14_data},
	{S2MPS11_LDO15, &s2m_ldo15_data},
	{S2MPS11_LDO16, &s2m_ldo16_data},
	{S2MPS11_LDO17, &s2m_ldo17_data},
	{S2MPS11_LDO20, &s2m_ldo20_data},
	{S2MPS11_LDO21, &s2m_ldo21_data},
	{S2MPS11_LDO22, &s2m_ldo22_data},
	{S2MPS11_LDO23, &s2m_ldo23_data},
	{S2MPS11_LDO24, &s2m_ldo24_data},
	{S2MPS11_LDO25, &s2m_ldo25_data},
	{S2MPS11_LDO26, &s2m_ldo26_data},
	{S2MPS11_LDO28, &s2m_ldo28_data},
	{S2MPS11_LDO29, &s2m_ldo29_data},
	{S2MPS11_LDO30, &s2m_ldo30_data},
	{S2MPS11_LDO32, &s2m_ldo32_data},
	{S2MPS11_LDO33, &s2m_ldo33_data},
	{S2MPS11_LDO34, &s2m_ldo34_data},
	{S2MPS11_LDO35, &s2m_ldo35_data},
	{S2MPS11_LDO37, &s2m_ldo37_data},
	{S2MPS11_LDO38, &s2m_ldo38_data},
};

struct sec_opmode_data s2mps11_opmode_data[S2MPS11_REG_MAX] = {
	[S2MPS11_BUCK1] = {S2MPS11_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK2] = {S2MPS11_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK3] = {S2MPS11_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK4] = {S2MPS11_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK5V123] = {S2MPS11_BUCK5V123, SEC_OPMODE_STANDBY},
	[S2MPS11_BUCK6] = {S2MPS11_BUCK6, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO5] = {S2MPS11_LDO5, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO6] = {S2MPS11_LDO6, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO7] = {S2MPS11_LDO7, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO8] = {S2MPS11_LDO8, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO9] = {S2MPS11_LDO9, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO10] = {S2MPS11_LDO10, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO11] = {S2MPS11_LDO11, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO12] = {S2MPS11_LDO12, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO13] = {S2MPS11_LDO13, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO15] = {S2MPS11_LDO15, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO17] = {S2MPS11_LDO17, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO20] = {S2MPS11_LDO20, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO21] = {S2MPS11_LDO21, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO22] = {S2MPS11_LDO22, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO24] = {S2MPS11_LDO24, SEC_OPMODE_STANDBY},
	[S2MPS11_LDO25] = {S2MPS11_LDO25, SEC_OPMODE_STANDBY},
};

static int sec_cfg_irq(void)
{
	/* AP_PMIC_IRQ: EINT7 */
	s3c_gpio_cfgpin(EXYNOS5410_GPX0(7), S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(EXYNOS5410_GPX0(7), S3C_GPIO_PULL_UP);
	return 0;
}

struct sec_pmic_platform_data exynos5_s2m_pdata = {
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
	.buck5_ramp_delay	= 12,
	.buck2_ramp_enable	= 1,
	.buck3_ramp_enable	= 1,
	.buck4_ramp_enable	= 1,
	.buck5_ramp_enable	= 1,
	.buck6_ramp_enable	= 1,
};

struct i2c_board_info hs_i2c_devs0[PMIC_I2C_DEVS_MAX] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data = &exynos5_s2m_pdata,
		.irq		= IRQ_EINT(7),
	},
};

static int __init sec_pmic_reinit(void)
{
#if defined(CONFIG_MACH_JA_KOR_SKT) || \
	defined(CONFIG_MACH_JA_KOR_KT) || \
	defined(CONFIG_MACH_JA_KOR_LGT)
	if (system_rev <= 0x3)
#else
	/* W/A to prevent voltage undershoot of BUCK5V123 under rev0.5 */
	if (system_rev <= 0x4)
#endif
		s2mps11_opmode_data[S2MPS11_BUCK5V123].mode = SEC_OPMODE_NORMAL;

	return 0;
}
core_initcall(sec_pmic_reinit);

#endif /* CONFIG_REGULATOR_S2MPS11 */
