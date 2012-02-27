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

#include <plat/cpu.h>

#include "control.h"
#include "omap_opp_data.h"
#include "pm.h"
#include "abb.h"

/*
 * Structures containing OMAP5430 voltage supported and various
 * voltage dependent data for each VDD.
 */

#define OMAP5_ON_VOLTAGE_UV				1040000
#define OMAP5_ONLP_VOLTAGE_UV			1040000
#define OMAP5_ON_VOLTAGE_MPU_UV			1220000
#define OMAP5_ONLP_VOLTAGE_MPU_UV		1220000
#define OMAP5_RET_VOLTAGE_UV			700000
#define OMAP5_OFF_VOLTAGE_UV			0

#define OMAP5430_VDD_MPU_OPP_LOW		1150000
#define OMAP5430_VDD_MPU_OPP_NOM		1150000
#define OMAP5430_VDD_MPU_OPP_HIGH		1220000
#define OMAP5430_VDD_MPU_OPP_SB			1220000

struct omap_volt_data omap54xx_vdd_mpu_volt_data[] = {
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_LOW, OMAP54XX_CONTROL_FUSE_MPU_OPP50, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_NOM, OMAP54XX_CONTROL_FUSE_MPU_OPP100, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_HIGH, OMAP54XX_CONTROL_FUSE_MPU_OPPTURBO, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_SB, OMAP54XX_CONTROL_FUSE_MPU_OPPNITRO, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPPNITRO, 0xfa, 0x27, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap54xx_mpu_vp_data = {
	.vddmin			= OMAP5_VP_MPU_VLIMITTO_VDDMIN,
	.vddmax			= OMAP5_VP_MPU_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap54xx_mpu_vc_data = {
	.on			= OMAP5_ON_VOLTAGE_MPU_UV,
	.onlp			= OMAP5_ONLP_VOLTAGE_MPU_UV,
	.ret			= OMAP5_RET_VOLTAGE_UV,
	.off			= OMAP5_OFF_VOLTAGE_UV,
};

#define OMAP5430_VDD_MM_OPP_LOW			1150000
#define OMAP5430_VDD_MM_OPP_NOM			1150000
#define OMAP5430_VDD_MM_OPP_OD			1200000

struct omap_volt_data omap54xx_vdd_mm_volt_data[] = {
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MM_OPP_LOW, OMAP54XX_CONTROL_FUSE_MM_OPP50, OMAP54XX_CONTROL_FUSE_MM_LVT_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MM_OPP_NOM, OMAP54XX_CONTROL_FUSE_MM_OPP100, OMAP54XX_CONTROL_FUSE_MM_LVT_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP5430_VDD_MM_OPP_OD, OMAP54XX_CONTROL_FUSE_MM_OPPTURBO, OMAP54XX_CONTROL_FUSE_MM_LVT_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

struct omap_vp_param omap54xx_mm_vp_data = {
	.vddmin			= OMAP5_VP_MM_VLIMITTO_VDDMIN,
	.vddmax			= OMAP5_VP_MM_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap54xx_mm_vc_data = {
	.on			= OMAP5_ON_VOLTAGE_UV,
	.onlp			= OMAP5_ONLP_VOLTAGE_UV,
	.ret			= OMAP5_RET_VOLTAGE_UV,
	.off			= OMAP5_OFF_VOLTAGE_UV,
};

#define OMAP5430_VDD_CORE_OPP_LOW		1150000
#define OMAP5430_VDD_CORE_OPP_NOM		1150000

struct omap_volt_data omap54xx_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP5430_VDD_CORE_OPP_LOW, OMAP54XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(OMAP5430_VDD_CORE_OPP_NOM, OMAP54XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

struct omap_vp_param omap54xx_core_vp_data = {
	.vddmin			= OMAP5_VP_CORE_VLIMITTO_VDDMIN,
	.vddmax			= OMAP5_VP_CORE_VLIMITTO_VDDMAX,
};

struct omap_vc_param omap54xx_core_vc_data = {
	.on			= OMAP5_ON_VOLTAGE_UV,
	.onlp			= OMAP5_ONLP_VOLTAGE_UV,
	.ret			= OMAP5_RET_VOLTAGE_UV,
	.off			= OMAP5_OFF_VOLTAGE_UV,
};

/* Dependency of domains are as follows for OMAP5430 (OPP based):
 *
 *	MPU	IVA	CORE
 *	50	50	50+
 *	50	100+	100
 *	100+	50	100
 *	100+	100+	100
 */

/* OMAP 5430 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap54xx_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_LOW, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_LOW},
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_NOM, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_HIGH, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_SB, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
};
struct omap_vdd_dep_info omap54xx_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap54xx_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap54xx_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 5430 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap54xx_vdd_mm_core_dep_data[] = {
	{.main_vdd_volt = OMAP5430_VDD_MM_OPP_LOW, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_LOW},
	{.main_vdd_volt = OMAP5430_VDD_MM_OPP_NOM, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP5430_VDD_MM_OPP_OD, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
};

struct omap_vdd_dep_info omap54xx_vddmm_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap54xx_vdd_mm_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap54xx_vdd_mm_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};


static struct omap_opp_def __initdata omap54xx_opp_def_list[] = {
#ifdef CONFIG_MACH_OMAP_5430ZEBU
	/* MPU OPP1 - OPPLOW */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 550000000, OMAP5430_VDD_MPU_OPP_LOW),
	/* MPU OPP2 - OPPNOM */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 1100000000, OMAP5430_VDD_MPU_OPP_NOM),
	/* MPU OPP3 - OPP-HIGH */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", false, 1500000000, OMAP5430_VDD_MPU_OPP_HIGH),
#else
	/* MPU OPP1 - OPPLOW */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 400000000, OMAP5430_VDD_MPU_OPP_LOW),
	/* MPU OPP2 - OPPNOM */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", true, 800000000, OMAP5430_VDD_MPU_OPP_NOM),
	/* MPU OPP3 - OPP-HIGH */
	OPP_INITIALIZER("mpu", "virt_dpll_mpu_ck", "mpu", false, 1100000000, OMAP5430_VDD_MPU_OPP_HIGH),
#endif
	/* L3 OPP1 - OPPLOW */
	OPP_INITIALIZER("l3_main_1", "virt_l3_ck", "core", true, 133000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* L3 OPP2 - OPPNOM */
	OPP_INITIALIZER("l3_main_1", "virt_l3_ck", "core", true, 266000000, OMAP5430_VDD_CORE_OPP_NOM),
	/* TODO: Uncomment the following once we have drivers for them */
#if 0
	/* FDIF OPP1 - OPPLOW */
	OPP_INITIALIZER("fdif", "fdif_fck", "core", true, 64000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* FDIF OPP3 - OPPNOM */
	OPP_INITIALIZER("fdif", "fdif_fck", "core", true, 128000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* HSI OPP1 - OPPLOW */
	OPP_INITIALIZER("hsi", "hsi_fck", "core", true, 96000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* FDIF OPP3 - OPPNOM */
	OPP_INITIALIZER("fdif", "hsi_fck", "core", true, 192000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* ABE OPP1 - OPPLOW */
	OPP_INITIALIZER("aess", "abe_clk", "core", true, 98304000, OMAP5430_VDD_CORE_OPP_LOW),
	/* ABE OPP2 - OPPNOM */
	OPP_INITIALIZER("aess", "abe_clk", "core", true, 196608000, OMAP5430_VDD_CORE_OPP_NOM),
#endif

#ifndef CONFIG_OMAP_PM_STANDALONE
	/* MMC1 OPP1 - OPPLOW */
	OPP_INITIALIZER("mmc1", "mmc1_fclk", "core", true, 96000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* MMC1 OPP2 - OPPNOM */
	OPP_INITIALIZER("mmc1", "mmc1_fclk", "core", true, 192000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* MMC2 OPP1 - OPPLOW */
	OPP_INITIALIZER("mmc2", "mmc2_fclk", "core", true, 96000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* MMC2 OPP2 - OPPNOM */
	OPP_INITIALIZER("mmc2", "mmc2_fclk", "core", true, 192000000, OMAP5430_VDD_CORE_OPP_NOM),


	/* IVA OPP1 - OPPLOW */
	OPP_INITIALIZER("iva", "dpll_iva_h12x2_ck", "mm", true, 194200000, OMAP5430_VDD_MM_OPP_LOW),
	/* IVA OPP2 - OPPNOM */
	OPP_INITIALIZER("iva", "dpll_iva_h12x2_ck", "mm", true, 388300000, OMAP5430_VDD_MM_OPP_NOM),
	/* IVA OPP3 - OPP-OD */
	OPP_INITIALIZER("iva", "dpll_iva_h12x2_ck", "mm", false, 532000000, OMAP5430_VDD_MM_OPP_OD),

	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER("dsp", "dpll_iva_h11x2_ck", "mm", true, 233000000, OMAP5430_VDD_MM_OPP_LOW),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER("dsp", "dpll_iva_h11x2_ck", "mm", true, 466000000, OMAP5430_VDD_MM_OPP_NOM),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER("dsp", "dpll_iva_h11x2_ck", "mm", false, 532000000, OMAP5430_VDD_MM_OPP_OD),
#endif
#if 0
	/* SGX OPP1 - OPPLOW */
	OPP_INITIALIZER("gpu", "dpll_per_h14x2_ck", "mm", true, 192000000, OMAP5430_VDD_MM_OPP_LOW),
	/* SGX OPP2 - OPPNOM */
	OPP_INITIALIZER("gpu", "dpll_per_h14x2_ck", "mm", true, 384000000, OMAP5430_VDD_MM_OPP_NOM),
	/* SGX OPP3 - OPPOV */
	OPP_INITIALIZER("gpu", "dpll_per_h14x2_ck", "mm", false, 532000000, OMAP5430_VDD_MM_OPP_OD),
#endif
};

/**
 * omap4_opp_init() - initialize omap4 opp table
 */
static int __init omap5_opp_init(void)
{
	int r = -ENODEV;

	if (!cpu_is_omap54xx())
		return r;

	r = omap_init_opp_table(omap54xx_opp_def_list,
			ARRAY_SIZE(omap54xx_opp_def_list));

	return r;
}
device_initcall(omap5_opp_init);
