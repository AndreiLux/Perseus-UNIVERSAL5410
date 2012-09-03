/*
 * OMAP4 OPP table definitions.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *	Kevin Hilman
 *	Thara Gopinath
 * Copyright (C) 2010-2011 Nokia Corporation.
 *      Eduardo Valentin
 *      Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/opp.h>

#include <plat/cpu.h>
#include <plat/common.h>

#include "control.h"
#include "omap_opp_data.h"
#include "pm.h"
#include "abb.h"

/*
 * STD_FUSE_OPP_DPLL_1 contains info about ABB trim type for MPU/IVA.
 * This probably is an ugly location to put the DPLL trim details.. but,
 * alternatives are even less attractive :( shrug..
 *
 * CONTROL_STD_FUSE_OPP_DPLL_1 bit fields:
 * Bit #|       Name       |        Description     |        Comment          |  Device
 * -----+------------------+------------------------+-------------------------|----------
 * 18-19|MPU_DPLL_TRIM_FREQ| 0 - 2.0GHz             | If RBB is not trimmed,  | OMAP4460
 *      |                  | 1 - 2.4GHz             | but MPU DPLL is trimmed |
 *      |                  | 2 - Reserved           | to 2.4GHz of higher,    |
 *      |                  | 3 - 3.0GHz             | it is recommended to    |
 *      |                  |                        | enable FBB for MPU at   |
 *      |                  |                        | OPPTB and OPPNT         |
 * -----+------------------+------------------------+-------------------------|----------
 *  20  |    MPU_RBB_TB    | 0 - RBB is trimmed     | If trimmed RBB can be   | OMAP4460/
 *      |                  | 1 - RBB is not trimmed | enabled at OPPTB on MPU | OMAP4470
 * -----+------------------+                        +-------------------------|----------
 *  21  |    IVA_RBB_TB    |                        | If trimmed RBB can be   | OMAP4460/
 *      |                  |                        | enabled at OPPTB on IVA | OMAP4470
 * -----+------------------+                        +-------------------------|----------
 *  22  |    MPU_RBB_NT    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPNT on MPU |
 * -----+------------------+                        +-------------------------|----------
 *  23  |    MPU_RBB_SB    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPSB on MPU |
 * -----+------------------+                        +-------------------------|----------
 *  24  |    IVA_RBB_NT    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPNT on IVA |
 * -----+------------------+                        +-------------------------|----------
 *  25  |    IVA_RBB_SB    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPSB on IVA |
 */
#define OMAP4460_MPU_OPP_DPLL_TRIM		BIT(18)
#define OMAP4460_MPU_OPP_DPLL_TURBO_RBB		BIT(20)
#define OMAP4460_IVA_OPP_DPLL_TURBO_RBB		BIT(21)
#define OMAP4470_MPU_OPP_DPLL_NITRO_RBB		BIT(22)
#define OMAP4470_MPU_OPP_DPLL_NITROSB_RBB	BIT(23)
#define OMAP4470_IVA_OPP_DPLL_NITRO_RBB		BIT(24)
#define OMAP4470_IVA_OPP_DPLL_NITROSB_RBB	BIT(25)

/**
 * struct omap4_ldo_abb_trim_data - describe ABB trim bits for specific voltage
 * @volt_data:		voltage table
 * @volt_nominal:	voltage for which ABB type should be modified according to trim bits.
 * @rbb_trim_mask:	If this bit is set in trim register, ABB type should be modified to RBB.
 * @fbb_trim_mask:	If this bit is set in trim register, ABB type should be modified to FBB.
 */
struct omap4_ldo_abb_trim_data {
	struct omap_volt_data *volt_data;
	u32 volt_nominal;
	u32 rbb_trim_mask;
	u32 fbb_trim_mask;
};

/*
 * Structures containing OMAP4430 voltage supported and various
 * voltage dependent data for each VDD.
 */

#define OMAP4_ON_VOLTAGE_UV			1375000
#define OMAP4_ONLP_VOLTAGE_UV			1375000
#define OMAP4_RET_VOLTAGE_UV			750000
#define OMAP4_OFF_VOLTAGE_UV			0

#define OMAP4460_ON_VOLTAGE_UV			1200000
#define OMAP4460_ONLP_VOLTAGE_UV		1200000
#define OMAP4460_RET_VOLTAGE_UV			750000
#define OMAP4460_OFF_VOLTAGE_UV			0

#define OMAP4430_VDD_MPU_OPP50_UV		1025000
#define OMAP4430_VDD_MPU_OPP100_UV		1200000
#define OMAP4430_VDD_MPU_OPPTURBO_UV		1313000
#define OMAP4430_VDD_MPU_OPPNITRO_UV		1375000

struct omap_volt_data omap443x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPP50_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPP100_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPTURBO_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPNITRO_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap443x_mpu_vp_data = {
	.vddmin			= OMAP4_VP_MPU_VLIMITTO_VDDMIN,
	.vddmax			= OMAP4_VP_MPU_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap44xx_mpu_vc_data = {
	.on			= OMAP4_ON_VOLTAGE_UV,
	.onlp			= OMAP4_ONLP_VOLTAGE_UV,
	.ret			= OMAP4_RET_VOLTAGE_UV,
	.off			= OMAP4_OFF_VOLTAGE_UV,
};

#define OMAP4430_VDD_IVA_OPP50_UV		1013000
#define OMAP4430_VDD_IVA_OPP100_UV		1188000
#define OMAP4430_VDD_IVA_OPPTURBO_UV		1300000

struct omap_volt_data omap443x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPP100_UV, 0, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPPTURBO_UV, 0, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap443x_iva_vp_data = {
	.vddmin			= OMAP4_VP_IVA_VLIMITTO_VDDMIN,
	.vddmax			= OMAP4_VP_IVA_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap44xx_iva_vc_data = {
	.on			= OMAP4_ON_VOLTAGE_UV,
	.onlp			= OMAP4_ONLP_VOLTAGE_UV,
	.ret			= OMAP4_RET_VOLTAGE_UV,
	.off			= OMAP4_OFF_VOLTAGE_UV,
};

#define OMAP4430_VDD_CORE_OPP50_UV		1025000
#define OMAP4430_VDD_CORE_OPP100_UV		1200000

struct omap_volt_data omap443x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_CORE_OPP100_UV, 0, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap443x_core_vp_data = {
	.vddmin			= OMAP4_VP_CORE_VLIMITTO_VDDMIN,
	.vddmax			= OMAP4_VP_CORE_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap44xx_core_vc_data = {
	.on			= OMAP4_ON_VOLTAGE_UV,
	.onlp			= OMAP4_ONLP_VOLTAGE_UV,
	.ret			= OMAP4_RET_VOLTAGE_UV,
	.off			= OMAP4_OFF_VOLTAGE_UV,
};

/* Dependency of domains are as follows for OMAP4430 (OPP based):
 *
 *	MPU	IVA	CORE
 *	50	50	50+
 *	50	100+	100
 *	100+	50	100
 *	100+	100+	100
 */

/* OMAP 4430 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap443x_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPP50_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPP100_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPPTURBO_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPPNITRO_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap443x_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap443x_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap443x_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 4430 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap443x_vdd_iva_core_dep_data[] = {
	{.main_vdd_volt = OMAP4430_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_IVA_OPPTURBO_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap443x_vddiva_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap443x_vdd_iva_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap443x_vdd_iva_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

static struct omap_opp_def __initdata omap443x_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER("mpu", "dpll_mpu_ck", "mpu", true, 300000000, OMAP4430_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER("mpu", "dpll_mpu_ck", "mpu", true, 600000000, OMAP4430_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER("mpu", "dpll_mpu_ck", "mpu", true, 800000000, OMAP4430_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-SB */
	OPP_INITIALIZER("mpu", "dpll_mpu_ck", "mpu", true, 1008000000, OMAP4430_VDD_MPU_OPPNITRO_UV),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OPP_INITIALIZER("l3_main_1", "virt_l3_ck", "core", true, 200000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER("iva", "virt_iva_ck", "iva", true, 266100000, OMAP4430_VDD_IVA_OPP100_UV),
	/* IVA OPP3 - OPP-Turbo */
	OPP_INITIALIZER("iva", "virt_iva_ck", "iva", false, 332000000, OMAP4430_VDD_IVA_OPPTURBO_UV),
#if 0
	/* ABE OPP1 - OPP50 */
        OPP_INITIALIZER("aess", "dpll_abe_x2_ck", "iva", true, 98304000, OMAP4430_VDD_IVA_OPP50_UV),
#endif
        /* ABE OPP2 - OPP100 */
        OPP_INITIALIZER("aess", "dpll_abe_x2_ck", "iva", true, 196608000, OMAP4430_VDD_IVA_OPP100_UV),
	/* TODO: add DSP, fdif, gpu */
};

#define OMAP4460_VDD_MPU_OPP50_UV		1025000
#define OMAP4460_VDD_MPU_OPP100_UV		1203000
#define OMAP4460_VDD_MPU_OPPTURBO_UV		1317000
#define OMAP4460_VDD_MPU_OPPNITRO_UV		1380000

struct omap_volt_data omap446x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPP50_UV, 10000, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf3, 0x11, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPP100_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf8, 0x24, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPTURBO_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x25, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPNITRO_UV, 0, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfb, 0x2f, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap446x_mpu_vp_data = {
	.vddmin			= OMAP4460_VP_MPU_VLIMITTO_VDDMIN,
	.vddmax			= OMAP4460_VP_MPU_VLIMITTO_VDDMAX,
};

#define OMAP4460_VDD_IVA_OPP50_UV		 950000
#define OMAP4460_VDD_IVA_OPP100_UV		1140000
#define OMAP4460_VDD_IVA_OPPTURBO_UV		1291000
#define OMAP4460_VDD_IVA_OPPNITRO_UV		1375000

struct omap_volt_data omap446x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPP50_UV, 13000, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPP100_UV, 0, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf8, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPTURBO_UV, 0, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPNITRO_UV, 0, OMAP44XX_CONTROL_FUSE_IVA_OPPNITRO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap446x_iva_vp_data = {
	.vddmin			= OMAP4460_VP_IVA_VLIMITTO_VDDMIN,
	.vddmax			= OMAP4460_VP_IVA_VLIMITTO_VDDMAX,
};

#define OMAP4460_VDD_CORE_OPP50_UV		 962000
#define OMAP4460_VDD_CORE_OPP100_UV		1127000
#define OMAP4460_VDD_CORE_OPP100_OV_UV		1250000

struct omap_volt_data omap446x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4460_VDD_CORE_OPP100_UV, 13000, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf8, 0x17, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap446x_core_vp_data = {
	.vddmin			= OMAP4460_VP_CORE_VLIMITTO_VDDMIN,
	.vddmax			= OMAP4460_VP_CORE_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap446x_core_vc_data = {
	.on			= OMAP4460_ON_VOLTAGE_UV,
	.onlp			= OMAP4460_ONLP_VOLTAGE_UV,
	.ret			= OMAP4460_RET_VOLTAGE_UV,
	.off			= OMAP4460_OFF_VOLTAGE_UV,
};

/* OMAP 4460 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap446x_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPP50_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPP100_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPPTURBO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPPNITRO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap446x_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap446x_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap446x_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 4460 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap446x_vdd_iva_core_dep_data[] = {
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPPTURBO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap446x_vddiva_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap446x_vdd_iva_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap446x_vdd_iva_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

static struct omap4_ldo_abb_trim_data __initdata omap446x_ldo_abb_trim_data[] = {
	{.volt_data = omap446x_vdd_mpu_volt_data, .volt_nominal = OMAP4460_VDD_MPU_OPPTURBO_UV, .rbb_trim_mask = OMAP4460_MPU_OPP_DPLL_TURBO_RBB,
			.fbb_trim_mask = OMAP4460_MPU_OPP_DPLL_TRIM},
	{.volt_data = omap446x_vdd_iva_volt_data, .volt_nominal = OMAP4460_VDD_IVA_OPPTURBO_UV, .rbb_trim_mask = OMAP4460_IVA_OPP_DPLL_TURBO_RBB},
	{.volt_data = NULL},
};

static struct omap_opp_def __initdata omap446x_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 350000000, OMAP4460_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 700000000, OMAP4460_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 920000000, OMAP4460_VDD_MPU_OPPTURBO_UV),
	/*
	 * MPU OPP4 - OPP-Nitro + Disabled as the reference schematics
	 * recommends TPS623631 - confirm and enable the opp in board file
	 * XXX: May be we should enable these based on mpu capability and
	 * Exception board files disable it...
	 */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", false, 1200000000, OMAP4460_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP4 - OPP-Nitro SpeedBin */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", false, 1500000000, OMAP4460_VDD_MPU_OPPNITRO_UV),
	/* L3 OPP2 - OPP100 */
	OPP_INITIALIZER("l3_main_1", "virt_l3_ck", "core", true, 200000000, OMAP4460_VDD_CORE_OPP100_UV),

	/* IVA OPP1 - OPP50 */
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER("iva", "virt_iva_ck", "iva", true, 266100000, OMAP4460_VDD_IVA_OPP100_UV),
	/*
	 * IVA OPP3 - OPP-Turbo + Disabled as the reference schematics
	 * recommends Phoenix VCORE2 which can supply only 600mA - so the ones
	 * above this OPP frequency, even though OMAP is capable, should be
	 * enabled by board file which is sure of the chip power capability
	 */
	OPP_INITIALIZER("iva", "virt_iva_ck", "iva", false, 332000000, OMAP4460_VDD_IVA_OPPTURBO_UV),
	/* IVA OPP4 - OPP-Nitro */
	OPP_INITIALIZER("iva", "virt_iva_ck", "iva", false, 430000000, OMAP4460_VDD_IVA_OPPNITRO_UV),
	/* IVA OPP5 - OPP-Nitro SpeedBin*/
	OPP_INITIALIZER("iva", "virt_iva_ck", "iva", false, 500000000, OMAP4460_VDD_IVA_OPPNITRO_UV),

#if 0
	/* SGX OPP1 - OPP50 */
	OPP_INITIALIZER("gpu", "dpll_per_m7x2_ck", "core", true, 153600000, OMAP4460_VDD_CORE_OPP50_UV),
	/* SGX OPP2 - OPP100 */
	OPP_INITIALIZER("gpu", "dpll_per_m7x2_ck", "core", true, 307200000, OMAP4460_VDD_CORE_OPP100_UV),
	/* SGX OPP3 - OPPOV */
	OPP_INITIALIZER("gpu", "dpll_per_m7x2_ck", "core", true, 384000000, OMAP4460_VDD_CORE_OPP100_OV_UV),
	/* FDIF OPP1 - OPP25 */
	OPP_INITIALIZER("fdif", "fdif_fck", "core", true, 32000000, OMAP4460_VDD_CORE_OPP50_UV),
	/* FDIF OPP2 - OPP50 */
	OPP_INITIALIZER("fdif", "fdif_fck", "core", true, 64000000, OMAP4460_VDD_CORE_OPP50_UV),
	/* FDIF OPP3 - OPP100 */
	OPP_INITIALIZER("fdif", "fdif_fck", "core", true, 128000000, OMAP4460_VDD_CORE_OPP100_UV),
	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER("dsp", "virt_dsp_ck", "iva", true, 232750000, OMAP4460_VDD_IVA_OPP50_UV),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER("dsp", "virt_dsp_ck", "iva", true, 465500000, OMAP4460_VDD_IVA_OPP100_UV),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER("dsp", "virt_dsp_ck", "iva", false, 496000000, OMAP4460_VDD_IVA_OPPTURBO_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER("hsi", "hsi_fck", "core", true, 96000000, OMAP4460_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER("hsi", "hsi_fck", "core", true, 96000000, OMAP4460_VDD_CORE_OPP100_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER("aess", "abe_clk", "iva", true, 98304000, OMAP4460_VDD_IVA_OPP100_UV),
#endif
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER("aess", "abe_clk", "iva", true, 196608000, OMAP4460_VDD_IVA_OPP100_UV),
};

/**
 * omap4_mpu_opp_enable() - helper to enable the OPP
 * @freq:	frequency to enable
 */
static void __init omap4_mpu_opp_enable(unsigned long freq)
{
	struct device *mpu_dev;
	int r;

	mpu_dev = omap2_get_mpuss_device();
	if (!mpu_dev) {
		pr_err("%s: no mpu_dev, did not enable f=%ld\n", __func__,
			freq);
		return;
	}

	r = opp_enable(mpu_dev, freq);
	if (r < 0)
		dev_err(mpu_dev, "%s: opp_enable failed(%d) f=%ld\n", __func__,
			r, freq);
}

/**
 * omap4_abb_update() - update the ABB map for a specific voltage in table
 * @vtable:	voltage table to update
 * @voltage:	voltage whose voltage data needs update
 * @abb_type:	what ABB type should we update it to?
 */
static void __init omap4_abb_update(struct omap_volt_data *vtable,
				    unsigned long voltage, int abb_type)
{
	/* scan through and update the voltage table */
	while (vtable->volt_nominal) {
		if (vtable->volt_nominal == voltage) {
			vtable->opp_sel = abb_type;
			return;
		}
		vtable++;
	}
	/* WARN noticably to get the developer to fix */
	WARN(1, "%s: voltage %ld could not be set to ABB %d\n",
	     __func__, voltage, abb_type);
}

/**
 * omap4_abb_trim_update() - update the ABB mapping quirks for OMAP4460/4470
 */
static void __init omap4_abb_trim_update(
		struct omap4_ldo_abb_trim_data *trim_data)
{
	u32 reg;
	int abb_type;

	if (!trim_data) {
		pr_err("%s: Trim data is not valid\n", __func__);
		return;
	}

	reg = omap_ctrl_readl(OMAP4_CTRL_MODULE_CORE_STD_FUSE_OPP_DPLL_1);

	/* Update ABB configuration if at least one of trim bits is set
	 * Leave default configuration in opposite case. */
	for (; trim_data->volt_data; trim_data++) {
		if (reg & trim_data->rbb_trim_mask)
			abb_type = OMAP_ABB_SLOW_OPP;
		else if (reg & trim_data->fbb_trim_mask)
			abb_type = OMAP_ABB_FAST_OPP;
		else
			continue;

		omap4_abb_update(trim_data->volt_data, trim_data->volt_nominal,
				abb_type);
	}
}

/**
 * omap4_opp_init() - initialize omap4 opp table
 */
int __init omap4_opp_init(void)
{
	int r = -ENODEV;

	if (!cpu_is_omap44xx())
		return r;
	if (cpu_is_omap443x())
		r = omap_init_opp_table(omap443x_opp_def_list,
			ARRAY_SIZE(omap443x_opp_def_list));
	else if (cpu_is_omap446x()) {
		omap4_abb_trim_update(omap446x_ldo_abb_trim_data);
		r = omap_init_opp_table(omap446x_opp_def_list,
			ARRAY_SIZE(omap446x_opp_def_list));
	}
	if (!r) {
		if (omap4_has_mpu_1_2ghz())
			omap4_mpu_opp_enable(1200000000);
		if (omap4_has_mpu_1_5ghz())
			omap4_mpu_opp_enable(1500000000);
	}

	return r;
}
device_initcall(omap4_opp_init);
