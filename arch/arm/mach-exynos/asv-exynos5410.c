/* linux/arch/arm/mach-exynos/asv-exynos5410.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5410 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos5410.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>

#ifdef CONFIG_SEC_PM
#include <linux/stat.h>
#include <mach/exynos-interface.h>
#endif

#define CHIP_ID3_REG		(S5P_VA_CHIPID + 0x04)
#define EXYNOS5410_IDS_OFFSET	(24)
#define EXYNOS5410_IDS_MASK	(0xFF)
#define EXYNOS5410_USESG_OFFSET	(3)
#define EXYNOS5410_USESG_MASK	(0x01)
#define EXYNOS5410_SG_OFFSET	(0)
#define EXYNOS5410_SG_MASK	(0x07)
#define EXYNOS5410_TABLE_OFFSET	(8)
#define EXYNOS5410_TABLE_MASK	(0x03)
#define EXYNOS5410_SG_A_OFFSET	(17)
#define EXYNOS5410_SG_A_MASK	(0x0F)
#define EXYNOS5410_SG_B_OFFSET	(21)
#define EXYNOS5410_SG_B_MASK	(0x03)
#define EXYNOS5410_SG_BSIGN_OFFSET	(23)
#define EXYNOS5410_SG_BSIGN_MASK	(0x01)

#define CHIP_ID4_REG		(S5P_VA_CHIPID + 0x1C)
#define EXYNOS5410_TMCB_OFFSET	(0)
#define EXYNOS5410_TMCB_MASK	(0x7F)
#define EXYNOS5410_EGLLOCK_UP_OFFSET	(8)
#define EXYNOS5410_EGLLOCK_UP_MASK	(0x03)
#define EXYNOS5410_EGLLOCK_DN_OFFSET	(10)
#define EXYNOS5410_EGLLOCK_DN_MASK	(0x03)
#define EXYNOS5410_KFCLOCK_UP_OFFSET	(12)
#define EXYNOS5410_KFCLOCK_UP_MASK	(0x03)
#define EXYNOS5410_KFCLOCK_DN_OFFSET	(14)
#define EXYNOS5410_KFCLOCK_DN_MASK	(0x03)
#define EXYNOS5410_INTLOCK_UP_OFFSET	(16)
#define EXYNOS5410_INTLOCK_UP_MASK	(0x03)
#define EXYNOS5410_INTLOCK_DN_OFFSET	(18)
#define EXYNOS5410_INTLOCK_DN_MASK	(0x03)
#define EXYNOS5410_MIFLOCK_UP_OFFSET	(20)
#define EXYNOS5410_MIFLOCK_UP_MASK	(0x03)
#define EXYNOS5410_MIFLOCK_DN_OFFSET	(22)
#define EXYNOS5410_MIFLOCK_DN_MASK	(0x03)
#define EXYNOS5410_G3DLOCK_UP_OFFSET	(24)
#define EXYNOS5410_G3DLOCK_UP_MASK	(0x03)
#define EXYNOS5410_G3DLOCK_DN_OFFSET	(26)
#define EXYNOS5410_G3DLOCK_DN_MASK	(0x03)

/* Following value use with *10000 */
#define EXYNOS5410_TMCB_CHIPER	10000
#define EXYNOS5410_MUL_VAL	9225
#define EXYNOS5410_MINUS_VAL	145520

#define LOT_ID_REG		(S5P_VA_CHIPID + 0x14)
#define LOT_ID_LEN		(5)

#define BASE_VOLTAGE_OFFSET	1000000

#if defined(CONFIG_TARGET_LOCALE_EUR) || defined(CONFIG_MACH_J_CHN_OPEN)
extern unsigned int system_rev;
#endif

enum table_version {
	ASV_TABLE_VER0,
	ASV_TABLE_VER1,
	ASV_TABLE_VER2,
	ASV_TABLE_VER3,
};

enum volt_offset {
	VOLT_OFFSET_0MV,
	VOLT_OFFSET_25MV,
	VOLT_OFFSET_50MV,
	VOLT_OFFSET_75MV,
};

bool is_special_lot;
bool is_speedgroup;
unsigned special_lot_group;
enum table_version asv_table_version;
enum volt_offset asv_volt_offset[5][2];

static const char *special_lot_list[] = {
	"NZXK8",
	"NZXKR",
	"NZXT6",
};

#ifdef CONFIG_ASV_MARGIN_TEST
static int set_arm_volt = 0;
static int set_kfc_volt = 0;
static int set_int_volt = 0;
static int set_mif_volt = 0;
static int set_g3d_volt = 0;

static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

static int __init get_kfc_volt(char *str)
{
	get_option(&str, &set_kfc_volt);
	return 0;
}
early_param("kfc", get_kfc_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);
#endif

unsigned int exynos5410_add_volt_offset(unsigned int voltage, enum volt_offset offset)
{
	switch (offset) {
	case VOLT_OFFSET_0MV:
		break;
	case VOLT_OFFSET_25MV:
		voltage += 25000;
		break;
	case VOLT_OFFSET_50MV:
		voltage += 50000;
		break;
	case VOLT_OFFSET_75MV:
		voltage += 75000;
		break;
	}

	return voltage;
}

unsigned int exynos5410_apply_volt_offset(unsigned int voltage, enum asv_type_id target_type)
{
	if (!is_speedgroup)
		return voltage;

	if (voltage > BASE_VOLTAGE_OFFSET)
		voltage = exynos5410_add_volt_offset(voltage, asv_volt_offset[target_type][0]);
	else
		voltage = exynos5410_add_volt_offset(voltage, asv_volt_offset[target_type][1]);

	return voltage;
}

void exynos5410_set_abb(struct asv_info *asv_inform)
{
	void __iomem *target_reg;
	unsigned int target_value;

	switch (asv_inform->asv_type) {
	case ID_ARM:
	case ID_KFC:
		target_reg = EXYNOS5410_BB_CON0;
		target_value = arm_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_INT_MIF_L0:
	case ID_INT_MIF_L1:
	case ID_INT_MIF_L2:
	case ID_INT_MIF_L3:
	case ID_MIF:
		target_reg = EXYNOS5410_BB_CON1;
		target_value = int_asv_abb_info[asv_inform->result_asv_grp];
		break;
	default:
		return;
	}

	set_abb(target_reg, target_value);
}

static unsigned int exynos5410_get_asv_group_arm(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_ARM);

	/* If sample is from special lot, must apply ASV group 0 */
	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5410_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	exynos5410_set_abb(asv_inform);

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(arm_asv_volt_info[i][target_asv_grp_nr + 1], ID_ARM) + set_arm_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(arm_asv_volt_info[i][target_asv_grp_nr + 1], ID_ARM);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_arm = {
	.get_asv_group	= exynos5410_get_asv_group_arm,
	.set_asv_info	= exynos5410_set_asv_info_arm,
};

static unsigned int exynos5410_get_asv_group_kfc(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_KFC);

	/* If sample is from special lot, must apply ASV group 0 */
	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5410_set_asv_info_kfc(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = kfc_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(kfc_asv_volt_info[i][target_asv_grp_nr + 1], ID_KFC) + set_kfc_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(kfc_asv_volt_info[i][target_asv_grp_nr + 1], ID_KFC);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_kfc = {
	.get_asv_group	= exynos5410_get_asv_group_kfc,
	.set_asv_info	= exynos5410_set_asv_info_kfc,
};

static unsigned int exynos5410_get_asv_group_int(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_INT_MIF_L0);

	/* If sample is from special lot, must apply ASV group 0 */
	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5410_set_asv_info_int_mif_lv0(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_mif_lv0_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv0_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT) + set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv0_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_int_mif_lv0 = {
	.get_asv_group	= exynos5410_get_asv_group_int,
	.set_asv_info	= exynos5410_set_asv_info_int_mif_lv0,
};

static void exynos5410_set_asv_info_int_mif_lvl(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_mif_lv1_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv1_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT) + set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv1_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_int_mif_lv1 = {
	.get_asv_group	= exynos5410_get_asv_group_int,
	.set_asv_info	= exynos5410_set_asv_info_int_mif_lvl,
};

static void exynos5410_set_asv_info_int_mif_lv2(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_mif_lv2_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv2_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT) + set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv2_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_int_mif_lv2 = {
	.get_asv_group	= exynos5410_get_asv_group_int,
	.set_asv_info	= exynos5410_set_asv_info_int_mif_lv2,
};

static void exynos5410_set_asv_info_int_mif_lv3(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_mif_lv3_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv3_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT) + set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(int_mif_lv3_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_int_mif_lv3 = {
	.get_asv_group	= exynos5410_get_asv_group_int,
	.set_asv_info	= exynos5410_set_asv_info_int_mif_lv3,
};

static unsigned int exynos5410_get_asv_group_mif(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_MIF);

	/* If sample is from special lot, must apply ASV group 0 */
	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}


	return 0;
}

static void exynos5410_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int offset = 0;

	exynos5410_set_abb(asv_inform);

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info[i][0];

#ifdef CONFIG_TARGET_LOCALE_EUR
		if (system_rev == 0xa) {
			if (i == 0)
				offset = 25000;
			else
				offset = 0;
		}
#endif

#ifdef CONFIG_TARGET_LOCALE_CHN
#ifdef CONFIG_MACH_J_CHN_OPEN
		if (system_rev == 0xa) {
			if (i == 0)
				offset = 25000;
			else
				offset = 0;
		}
#else
		if (i == 0)
			offset = 25000;
		else
			offset = 0;
#endif
#endif

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(mif_asv_volt_info[i][target_asv_grp_nr + 1], ID_MIF) + set_mif_volt + offset;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(mif_asv_volt_info[i][target_asv_grp_nr + 1], ID_MIF) + offset;
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_mif = {
	.get_asv_group	= exynos5410_get_asv_group_mif,
	.set_asv_info	= exynos5410_set_asv_info_mif,
};

static unsigned int exynos5410_get_asv_group_g3d(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_G3D);

	/* If sample is from special lot, must apply ASV group 0 */
	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5410_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(g3d_asv_volt_info[i][target_asv_grp_nr + 1], ID_G3D) + set_g3d_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5410_apply_volt_offset(g3d_asv_volt_info[i][target_asv_grp_nr + 1], ID_G3D);
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

struct asv_ops exynos5410_asv_ops_g3d = {
	.get_asv_group	= exynos5410_get_asv_group_g3d,
	.set_asv_info	= exynos5410_set_asv_info_g3d,
};

struct asv_info exynos5410_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5410_asv_ops_arm,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5410_asv_ops_kfc,
		.asv_group_nr	= ASV_GRP_NR(KFC),
		.dvfs_level_nr	= DVFS_LEVEL_NR(KFC),
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT_MIF_L0,
		.name		= "VDD_INT_MIF_L0",
		.ops		= &exynos5410_asv_ops_int_mif_lv0,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos5410_asv_ops_mif,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos5410_asv_ops_g3d,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	}, {
		.asv_type	= ID_INT_MIF_L1,
		.name		= "VDD_INT_MIF_L1",
		.ops		= &exynos5410_asv_ops_int_mif_lv1,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_INT_MIF_L2,
		.name		= "VDD_INT_MIF_L2",
		.ops		= &exynos5410_asv_ops_int_mif_lv2,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_INT_MIF_L3,
		.name		= "VDD_INT_MIF_L3",
		.ops		= &exynos5410_asv_ops_int_mif_lv3,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	},
};

unsigned int exynos5410_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos5410_asv_member); i++)
		add_asv_member(&exynos5410_asv_member[i]);

	return 0;
}

static void exynos5410_check_lot_id(struct asv_common *asv_info)
{
	unsigned int lid_reg = 0;
	unsigned int rev_lid = 0;
	unsigned int i;
	unsigned int tmp;

	lid_reg = __raw_readl(LOT_ID_REG);

	for (i = 0; i < 32; i++) {
		tmp = (lid_reg >> i) & 0x1;
		rev_lid += tmp << (31 - i);
	}

	asv_info->lot_name[0] = 'N';
	lid_reg = (rev_lid >> 11) & 0x1FFFFF;

	for (i = 4; i >= 1; i--) {
		tmp = lid_reg % 36;
		lid_reg /= 36;
		asv_info->lot_name[i] = (tmp < 10) ? (tmp + '0') : ((tmp - 10) + 'A');
	}

	for (i = 0; i < ARRAY_SIZE(special_lot_list); i++) {
		if (!strncmp(asv_info->lot_name, special_lot_list[i], LOT_ID_LEN)) {
			is_special_lot = true;
			goto out;
		}
	}

	is_special_lot = false;
out:
	pr_info("Exynos5410 : Lot ID is %s[%s]\n", asv_info->lot_name,
				(is_special_lot ? "Special" : "Non Special"));
}

#ifdef CONFIG_SEC_PM
static ssize_t show_asv_group_info(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	unsigned int i;
	int ids_value = 0, hpm_value = 0;
	int asv_group = 0, ids_group = 0, hpm_group = 0;
	unsigned int chip_id3_value;
	unsigned int chip_id4_value;
	char *asv_grade;
	struct clk *clk_chipid;
	int count = 0;

	clk_chipid = clk_get(NULL, "chipid_apbif");
	if (IS_ERR(clk_chipid)) {
		pr_info("EXYNOS5410 ASV : cannot find chipid clock!\n");
		return -EINVAL;
	}

	clk_enable(clk_chipid);

	chip_id3_value = __raw_readl(CHIP_ID3_REG);
	chip_id4_value = __raw_readl(CHIP_ID4_REG);
	ids_value = (chip_id3_value >> EXYNOS5410_IDS_OFFSET) & EXYNOS5410_IDS_MASK;
	hpm_value = (chip_id4_value >> EXYNOS5410_TMCB_OFFSET) & EXYNOS5410_TMCB_MASK;

	clk_disable(clk_chipid);

	for (i = 0; i < ASV_GRP_NR(ARM); i++) {
		if (refer_use_table_get_asv[0][i] &&
				ids_value <= refer_table_get_asv[0][i]) {
			ids_group = i;
			break;
		}
	}

	for (i = 0; i < ASV_GRP_NR(ARM); i++) {
		if (refer_use_table_get_asv[1][i] &&
				hpm_value <= refer_table_get_asv[1][i]) {
			hpm_group = i;
			break;
		}
	}

	if (is_speedgroup)
		asv_group = special_lot_group;
	else
		asv_group = min(ids_group, hpm_group);

	asv_grade = "B";
	if (asv_group >= 2 && asv_group <= 9)
		if ((hpm_group - ids_group) >= 0)
			asv_grade = "A";

	if ((ids_value > 100) || (ids_value <= 9))
		asv_grade = "C";

	pr_info("ASV %d / IDS %d , %d / TMCB %d, %d / TMCB-IDS skew %d / Group %s\n",
			asv_group, ids_value, ids_group, hpm_value, hpm_group,
			hpm_group - ids_group, asv_grade);

	/* Set asv group info to buf */
	count += sprintf(&buf[count], "%d ", asv_group);
	count += sprintf(&buf[count], "%d ", ids_value);
	count += sprintf(&buf[count], "%d ", ids_group);
	count += sprintf(&buf[count], "%d ", hpm_value);
	count += sprintf(&buf[count], "%d ", hpm_group);
	count += sprintf(&buf[count], "%d ", hpm_group-ids_group);
	count += sprintf(&buf[count], "%s ", asv_grade);

	count += sprintf(&buf[count], "\n");
	return count;
}

static struct sysfs_attr asv_group_info =
		__ATTR(asv_group_info, S_IRUGO,
			show_asv_group_info, NULL);
#endif /* CONFIG_SEC_PM */

int exynos5410_init_asv(struct asv_common *asv_info)
{
	struct clk *clk_chipid;
	unsigned int chip_id3_value;
	unsigned int chip_id4_value;
#ifdef CONFIG_SEC_PM
	int ret = 0;
#endif

	special_lot_group = 0;
	is_special_lot = false;
	is_speedgroup = false;

	/* lot ID Check */
	clk_chipid = clk_get(NULL, "chipid_apbif");
	if (IS_ERR(clk_chipid)) {
		pr_info("EXYNOS5410 ASV : cannot find chipid clock!\n");
		return -EINVAL;
	}

	clk_enable(clk_chipid);
	chip_id3_value = __raw_readl(CHIP_ID3_REG);
	chip_id4_value = __raw_readl(CHIP_ID4_REG);

	exynos5410_check_lot_id(asv_info);

	if (is_special_lot)
		goto set_asv_info;

	if ((chip_id3_value >> EXYNOS5410_USESG_OFFSET) & EXYNOS5410_USESG_MASK) {
		if (!((chip_id3_value >> EXYNOS5410_SG_BSIGN_OFFSET) & EXYNOS5410_SG_BSIGN_MASK))
			special_lot_group = ((chip_id3_value >> EXYNOS5410_SG_A_OFFSET) & EXYNOS5410_SG_A_MASK)
					- ((chip_id3_value >> EXYNOS5410_SG_B_OFFSET) & EXYNOS5410_SG_B_MASK);
		else
			special_lot_group = ((chip_id3_value >> EXYNOS5410_SG_A_OFFSET) & EXYNOS5410_SG_A_MASK)
					+ ((chip_id3_value >> EXYNOS5410_SG_B_OFFSET) & EXYNOS5410_SG_B_MASK);
		is_speedgroup = true;
		pr_info("Exynos5410 ASV : Use Fusing Speed Group %d\n", special_lot_group);
	} else {
		asv_info->hpm_value = (chip_id4_value >> EXYNOS5410_TMCB_OFFSET) & EXYNOS5410_TMCB_MASK;
		asv_info->ids_value = (chip_id3_value >> EXYNOS5410_IDS_OFFSET) & EXYNOS5410_IDS_MASK;
	}

	if (!asv_info->hpm_value) {
		is_special_lot = true;
		pr_info("Exynos5410 ASV : invalid IDS value\n");
	}

	pr_info("EXYNOS5410 ASV : %s IDS : %d HPM : %d\n", asv_info->lot_name,
				asv_info->ids_value, asv_info->hpm_value);

	asv_table_version = (chip_id3_value >> EXYNOS5410_TABLE_OFFSET) & EXYNOS5410_TABLE_MASK;
	asv_volt_offset[ID_ARM][0] = (chip_id4_value >> EXYNOS5410_EGLLOCK_UP_OFFSET) & EXYNOS5410_EGLLOCK_UP_MASK;
	asv_volt_offset[ID_ARM][1] = (chip_id4_value >> EXYNOS5410_EGLLOCK_DN_OFFSET) & EXYNOS5410_EGLLOCK_DN_MASK;
	asv_volt_offset[ID_KFC][0] = (chip_id4_value >> EXYNOS5410_KFCLOCK_UP_OFFSET) & EXYNOS5410_KFCLOCK_UP_MASK;
	asv_volt_offset[ID_KFC][1] = (chip_id4_value >> EXYNOS5410_KFCLOCK_DN_OFFSET) & EXYNOS5410_KFCLOCK_DN_MASK;
	asv_volt_offset[ID_INT][0] = (chip_id4_value >> EXYNOS5410_INTLOCK_UP_OFFSET) & EXYNOS5410_INTLOCK_UP_MASK;
	asv_volt_offset[ID_INT][1] = (chip_id4_value >> EXYNOS5410_INTLOCK_DN_OFFSET) & EXYNOS5410_INTLOCK_DN_MASK;
	asv_volt_offset[ID_G3D][0] = (chip_id4_value >> EXYNOS5410_G3DLOCK_UP_OFFSET) & EXYNOS5410_G3DLOCK_UP_MASK;
	asv_volt_offset[ID_G3D][1] = (chip_id4_value >> EXYNOS5410_G3DLOCK_DN_OFFSET) & EXYNOS5410_G3DLOCK_DN_MASK;
	asv_volt_offset[ID_MIF][0] = (chip_id4_value >> EXYNOS5410_MIFLOCK_UP_OFFSET) & EXYNOS5410_MIFLOCK_UP_MASK;
	asv_volt_offset[ID_MIF][1] = (chip_id4_value >> EXYNOS5410_MIFLOCK_DN_OFFSET) & EXYNOS5410_MIFLOCK_DN_MASK;

set_asv_info:
	clk_disable(clk_chipid);

	asv_info->regist_asv_member = exynos5410_regist_asv_member;

#ifdef CONFIG_SEC_PM
	ret = sysfs_create_file(power_kobj, &asv_group_info.attr);
	if (ret)
		return ret;
#endif

	return 0;
}
