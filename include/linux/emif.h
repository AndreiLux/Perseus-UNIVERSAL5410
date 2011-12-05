/*
 * Definitions for EMIF SDRAM controller in TI SoCs
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Aneesh V <aneesh@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_EMIF_H
#define __LINUX_EMIF_H

#ifndef __ASSEMBLY__
#include <linux/jedec_ddr.h>

/*
 * Maximum number of different frequencies supported by EMIF driver
 * Determines the number of entries in the pointer array for register
 * cache
 */
#define EMIF_MAX_NUM_FREQUENCIES	6

/* State of the core voltage */
#define DDR_VOLTAGE_STABLE	0
#define DDR_VOLTAGE_RAMPING	1

/* Defines for timing De-rating */
#define EMIF_NORMAL_TIMINGS	0
#define EMIF_DERATED_TIMINGS	1

/* Length of the forced read idle period in terms of cycles */
#define EMIF_READ_IDLE_LEN_VAL	5

/*
 * forced read idle interval to be used when voltage
 * is changed as part of DVFS/DPS - 1ms
 */
#define READ_IDLE_INTERVAL_DVFS	(1*1000000)

/*
 * Forced read idle interval to be used when voltage is stable
 * 50us - or maximum value will do
 */
#define READ_IDLE_INTERVAL_NORMAL	(50*1000000)

/* DLL calibration interval when voltage is NOT stable - 1us */
#define DLL_CALIB_INTERVAL_DVFS		(1*1000000)

#define DLL_CALIB_ACK_WAIT_VAL		5

/* Interval between ZQCS commands - hw team recommended value */
#define EMIF_ZQCS_INTERVAL_US	(50*1000)
/* Enable ZQ Calibration on exiting Self-refresh */
#define ZQ_SFEXITEN_ENABLE		1
/*
 * ZQ Calibration simultaneously on both chip-selects:
 * Needs one calibration resistor per CS
 */
#define	ZQ_DUALCALEN_DISABLE	0
#define	ZQ_DUALCALEN_ENABLE		1

#define T_ZQCS_DEFAULT_NS	90
#define T_ZQCL_DEFAULT_NS	360
#define T_ZQINIT_DEFAULT_NS	1000

/* EMIF_PWR_MGMT_CTRL register */
/* Low power modes */
#define EMIF_LP_MODE_DISABLE		0
#define EMIF_LP_MODE_CLOCK_STOP		1
#define EMIF_LP_MODE_SELF_REFRESH	2
#define EMIF_LP_MODE_PWR_DN		4

/* DPD_EN */
#define DPD_DISABLE	0
#define DPD_ENABLE	1

/*
 * Default values for the low-power entry to be used if not provided by user.
 * OMAP4/5 has a hw bug(i735) due to which this value can not be less than 512
 */
#define EMIF_LP_MODE_TIMEOUT_PERFORMANCE	2048		/* in cycles */
#define EMIF_LP_MODE_TIMEOUT_POWER		512		/* in cycles */
#define EMIF_LP_MODE_FREQ_THRESHOLD		400000000	/* 400 MHz */

/* DDR_PHY_CTRL_1 magic values for OMAP4 - suggested by hw team */
#define EMIF_DDR_PHY_CTRL_1_BASE_VAL_ATTILAPHY			0x049FF000
#define EMIF_DLL_SLAVE_DLY_CTRL_400_MHZ_ATTILAPHY		0x41
#define EMIF_DLL_SLAVE_DLY_CTRL_200_MHZ_ATTILAPHY		0x80
#define EMIF_DLL_SLAVE_DLY_CTRL_100_MHZ_AND_LESS_ATTILAPHY	0xFF

/* DDR_PHY_CTRL_1 values for OMAP5 */
#define EMIF_DDR_PHY_CTRL_1_BASE_VAL_INTELLIPHY		0x0E084200
#define EMIF_PHY_TOTAL_READ_LATENCY_INTELLIPHY_PS	10000 /* in ps */

/* TEMP_ALERT_CONFIG */
#define TEMP_ALERT_POLL_INTERVAL_DEFAULT_MS	360 /* Temp gradient - 5 C/s */

#define EMIF_T_CSTA			3
#define EMIF_T_PDLL_UL			128

/* Hardware capabilities */
#define	EMIF_HW_CAPS_LL_INTERFACE	0x00000001

/* EMIF IP Revisions */
#define	EMIF_4D		1
#define	EMIF_4D5	2

/* PHY types */
#define	EMIF_PHY_TYPE_ATTILAPHY		1
#define	EMIF_PHY_TYPE_INTELLIPHY	2

/* Custom config requests */
#define EMIF_CUSTOM_CONFIG_LPMODE			0x00000001
#define EMIF_CUSTOM_CONFIG_TEMP_ALERT_POLL_INTERVAL	0x00000002

/* External PHY control registers magic values */
#define EMIF_EXT_PHY_CTRL_1_VAL		0x04020080
#define EMIF_EXT_PHY_CTRL_5_VAL		0x04010040
#define EMIF_EXT_PHY_CTRL_6_VAL		0x01004010
#define EMIF_EXT_PHY_CTRL_7_VAL		0x00001004
#define EMIF_EXT_PHY_CTRL_8_VAL		0x04010040
#define EMIF_EXT_PHY_CTRL_9_VAL		0x01004010
#define EMIF_EXT_PHY_CTRL_10_VAL	0x00001004
#define EMIF_EXT_PHY_CTRL_11_VAL	0x00000000
#define EMIF_EXT_PHY_CTRL_12_VAL	0x00000000
#define EMIF_EXT_PHY_CTRL_13_VAL	0x00000000
#define EMIF_EXT_PHY_CTRL_14_VAL	0x80080080
#define EMIF_EXT_PHY_CTRL_15_VAL	0x00800800
#define EMIF_EXT_PHY_CTRL_16_VAL	0x08102040
#define EMIF_EXT_PHY_CTRL_17_VAL	0x00000001
#define EMIF_EXT_PHY_CTRL_18_VAL	0x540A8150
#define EMIF_EXT_PHY_CTRL_19_VAL	0xA81502A0
#define EMIF_EXT_PHY_CTRL_20_VAL	0x002A0540
#define EMIF_EXT_PHY_CTRL_21_VAL	0x00000000
#define EMIF_EXT_PHY_CTRL_22_VAL	0x00000000
#define EMIF_EXT_PHY_CTRL_23_VAL	0x00000000
#define EMIF_EXT_PHY_CTRL_24_VAL	0x00000077

#define EMIF_INTELLI_PHY_DQS_GATE_OPENING_DELAY_PS	1200

/*
 * Structure containing shadow of important registers in EMIF
 * The calculation function fills in this structure to be later used for
 * initialisation and DVFS
 */
struct emif_regs {
	u32 freq;
	u32 ref_ctrl_shdw;
	u32 ref_ctrl_shdw_derated;
	u32 sdram_tim1_shdw;
	u32 sdram_tim1_shdw_derated;
	u32 sdram_tim2_shdw;
	u32 sdram_tim3_shdw;
	u32 sdram_tim3_shdw_derated;
	u32 pwr_mgmt_ctrl_shdw;
	union {
		u32 read_idle_ctrl_shdw_normal;
		u32 dll_calib_ctrl_shdw_normal;
	};
	union {
		u32 read_idle_ctrl_shdw_volt_ramp;
		u32 dll_calib_ctrl_shdw_volt_ramp;
	};

	u32 phy_ctrl_1_shdw;
	u32 ext_phy_ctrl_2_shdw;
	u32 ext_phy_ctrl_3_shdw;
	u32 ext_phy_ctrl_4_shdw;
};

/** struct ddr_device_info - All information about the DDR device except AC
 *		timing parameters
 * @type:	Device type (LPDDR2-S4, LPDDR2-S2 etc)
 * @density:	Device density
 * @io_width:	Bus width
 * @cs1_used:	Whether there is a DDR device attached to the second
 *		chip-select(CS1) of this EMIF instance
 * @cal_resistors_per_cs: Whether there is one calibration resistor per
 *		chip-select or whether it's a single one for both
 * @manufacturer: Manufacturer name string
 */
struct ddr_device_info {
	u32	type;
	u32	density;
	u32	io_width;
	u32	cs1_used;
	u32	cal_resistors_per_cs;
	char	manufacturer[10];
};

/**
 * struct emif_custom_configs - Custom configuration parameters/policies
 *		passed from the platform layer
 * @mask:	Mask to indicate which configs are requested
 * @lpmode:	LPMODE to be used in PWR_MGMT_CTRL register
 * @lpmode_timeout_performance: Timeout before LPMODE entry when higher
 *		performance is desired at the cost of power (typically
 *		at higher OPPs)
 * @lpmode_timeout_power: Timeout before LPMODE entry when better power
 *		savings is desired and performance is not important
 *		(typically at lower loads indicated by lower OPPs)
 * @lpmode_freq_threshold: The DDR frequency threshold to identify between
 *		the above two cases:
 *		timeout = (freq >= lpmode_freq_threshold) ?
 *			lpmode_timeout_performance :
 *			lpmode_timeout_power;
 * @temp_alert_poll_interval_ms: LPDDR2 MR4 polling interval at nominal
 *		temperature(in milliseconds). When temperature is high
 *		polling is done 4 times as frequently.
 */
struct emif_custom_configs {
	u32 mask;
	u32 lpmode;
	u32 lpmode_timeout_performance;
	u32 lpmode_timeout_power;
	u32 lpmode_freq_threshold;
	u32 temp_alert_poll_interval_ms;
};

/**
 * struct emif_platform_data - Platform data passed on EMIF platform
 *				device creation. Used by the driver.
 * @hw_caps:		Hw capabilities of the EMIF IP in the respective SoC
 * @device_info:	Device info structure containing information such
 *			as type, bus width, density etc
 * @timings:		Timings information from device datasheet passed
 *			as an array of 'struct lpddr2_timings'. Can be NULL
 *			if if default timings are ok
 * @timings_arr_size:	Size of the timings array. Depends on the number
 *			of different frequencies for which timings data
 *			is provided
 * @min_tck:		Minimum value of some timing parameters in terms
 *			of number of cycles. Can be NULL if default values
 *			are ok
 * @custom_configs:	Custom configurations requested by SoC or board
 *			code and the data for them. Can be NULL if default
 *			configurations done by the driver are ok. See
 *			documentation for 'struct emif_custom_configs' for
 *			more details
 */
struct emif_platform_data {
	u32 hw_caps;
	struct ddr_device_info *device_info;
	const struct lpddr2_timings *timings;
	u32 timings_arr_size;
	const struct ddr_min_tck *min_tck;
	struct emif_custom_configs *custom_configs;
	u32 ip_rev;
	u32 phy_type;
};

void emif_freq_pre_notify_handler(u32 new_freq);
#endif /* __ASSEMBLY__ */

#endif /* __LINUX_EMIF_H */
