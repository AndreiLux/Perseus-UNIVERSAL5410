/* linux/arch/arm/mach-exynos/asv-exynos5420.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5420 - ASV(Adoptive Support Voltage) driver
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
#include <mach/asv-exynos5420.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <linux/regulator/consumer.h>

#include <plat/cpu.h>

#define CHIP_ID3_REG		(S5P_VA_CHIPID + 0x04)
#define EXYNOS5420_IDS_OFFSET	(24)
#define EXYNOS5420_IDS_MASK	(0xFF)
#define EXYNOS5420_USESG_OFFSET	(3)
#define EXYNOS5420_USESG_MASK	(0x01)
#define EXYNOS5420_SG_OFFSET	(0)
#define EXYNOS5420_SG_MASK	(0x07)
#define EXYNOS5420_TABLE_OFFSET	(8)
#define EXYNOS5420_TABLE_MASK	(0x03)
#define EXYNOS5420_SG_A_OFFSET	(17)
#define EXYNOS5420_SG_A_MASK	(0x0F)
#define EXYNOS5420_SG_B_OFFSET	(21)
#define EXYNOS5420_SG_B_MASK	(0x03)
#define EXYNOS5420_SG_BSIGN_OFFSET	(23)
#define EXYNOS5420_SG_BSIGN_MASK	(0x01)

#define CHIP_ID4_REG		(S5P_VA_CHIPID + 0x1C)
#define EXYNOS5420_TMCB_OFFSET	(0)
#define EXYNOS5420_TMCB_MASK	(0x7F)
#define EXYNOS5420_EGLLOCK_UP_OFFSET	(8)
#define EXYNOS5420_EGLLOCK_UP_MASK	(0x03)
#define EXYNOS5420_EGLLOCK_DN_OFFSET	(10)
#define EXYNOS5420_EGLLOCK_DN_MASK	(0x03)
#define EXYNOS5420_KFCLOCK_UP_OFFSET	(12)
#define EXYNOS5420_KFCLOCK_UP_MASK	(0x03)
#define EXYNOS5420_KFCLOCK_DN_OFFSET	(14)
#define EXYNOS5420_KFCLOCK_DN_MASK	(0x03)
#define EXYNOS5420_INTLOCK_UP_OFFSET	(16)
#define EXYNOS5420_INTLOCK_UP_MASK	(0x03)
#define EXYNOS5420_INTLOCK_DN_OFFSET	(18)
#define EXYNOS5420_INTLOCK_DN_MASK	(0x03)
#define EXYNOS5420_MIFLOCK_UP_OFFSET	(20)
#define EXYNOS5420_MIFLOCK_UP_MASK	(0x03)
#define EXYNOS5420_MIFLOCK_DN_OFFSET	(22)
#define EXYNOS5420_MIFLOCK_DN_MASK	(0x03)
#define EXYNOS5420_G3DLOCK_UP_OFFSET	(24)
#define EXYNOS5420_G3DLOCK_UP_MASK	(0x03)
#define EXYNOS5420_G3DLOCK_DN_OFFSET	(26)
#define EXYNOS5420_G3DLOCK_DN_MASK	(0x03)

/* Following value use with *10000 */
#define EXYNOS5420_TMCB_CHIPER	10000
#define EXYNOS5420_MUL_VAL	9225
#define EXYNOS5420_MINUS_VAL	145520

#define LOT_ID_REG		(S5P_VA_CHIPID + 0x14)
#define LOT_ID_LEN		(5)

#define BASE_VOLTAGE_OFFSET	1000000

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

bool is_speedgroup;
unsigned special_lot_group;
enum table_version asv_table_version;
enum volt_offset asv_volt_offset[5][2];

struct asv_reference {
	unsigned int ids_value;
	unsigned int hpm_value;
	bool is_speedgroup;
};
static struct asv_reference asv_ref_info = {0, 0, false};

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

unsigned int exynos5420_add_volt_offset(unsigned int voltage, enum volt_offset offset)
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

static unsigned int exynos5420_apply_volt_offset(unsigned int voltage, enum asv_type_id target_type)
{
	if (!is_speedgroup)
		return voltage;

	if (voltage > BASE_VOLTAGE_OFFSET)
		voltage = exynos5420_add_volt_offset(voltage, asv_volt_offset[target_type][0]);
	else
		voltage = exynos5420_add_volt_offset(voltage, asv_volt_offset[target_type][1]);

	return voltage;
}

static void exynos5420_set_abb(struct asv_info *asv_inform)
{
	void __iomem *target_reg;
	unsigned int target_value;

	switch (asv_inform->asv_type) {
	case ID_ARM:
		target_reg = EXYNOS5420_BIAS_CON_ARM;
		target_value = arm_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_KFC:
		target_reg = EXYNOS5420_BIAS_CON_KFC;
		target_value = kfc_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_INT:
		target_reg = EXYNOS5420_BIAS_CON_INT;
		target_value = int_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_MIF:
		target_reg = EXYNOS5420_BIAS_CON_MIF;
		target_value = mif_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_G3D:
		target_reg = EXYNOS5420_BIAS_CON_G3D;
		target_value = g3d_asv_abb_info[asv_inform->result_asv_grp];
		break;
	default:
		return;
	}

	set_abb(target_reg, target_value);
}

static unsigned int exynos5420_get_asv_group_arm(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_ARM);

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

static void exynos5420_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	exynos5420_set_abb(asv_inform);

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info_evt1[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(arm_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_ARM)
			+ set_arm_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(arm_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_ARM);
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

static struct asv_ops exynos5420_asv_ops_arm = {
	.get_asv_group	= exynos5420_get_asv_group_arm,
	.set_asv_info	= exynos5420_set_asv_info_arm,
};

static unsigned int exynos5420_get_asv_group_kfc(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_KFC);

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

static void exynos5420_set_asv_info_kfc(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = kfc_asv_volt_info_evt1[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(kfc_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_KFC)
			+ set_kfc_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(kfc_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_KFC);
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

static struct asv_ops exynos5420_asv_ops_kfc = {
	.get_asv_group	= exynos5420_get_asv_group_kfc,
	.set_asv_info	= exynos5420_set_asv_info_kfc,
};

static unsigned int exynos5420_get_asv_group_int(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_INT);

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

static void exynos5420_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_asv_volt_info_evt1[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(int_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_INT)
			+ set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(int_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_INT);
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

static struct asv_ops exynos5420_asv_ops_int = {
	.get_asv_group	= exynos5420_get_asv_group_int,
	.set_asv_info	= exynos5420_set_asv_info_int,
};

static unsigned int exynos5420_get_asv_group_mif(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_MIF);

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

static void exynos5420_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	exynos5420_set_abb(asv_inform);

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info_evt1[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(mif_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_MIF)
			+ set_mif_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(mif_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_MIF);
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

static struct asv_ops exynos5420_asv_ops_mif = {
	.get_asv_group	= exynos5420_get_asv_group_mif,
	.set_asv_info	= exynos5420_set_asv_info_mif,
};

static unsigned int exynos5420_get_asv_group_g3d(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_G3D);

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

static void exynos5420_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info_evt1[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(g3d_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_G3D)
			+ set_g3d_volt;
#else
		asv_inform->asv_volt[i].asv_value =
			exynos5420_apply_volt_offset(g3d_asv_volt_info_evt1[i][target_asv_grp_nr + 1], ID_G3D);
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

static struct asv_ops exynos5420_asv_ops_g3d = {
	.get_asv_group	= exynos5420_get_asv_group_g3d,
	.set_asv_info	= exynos5420_set_asv_info_g3d,
};

struct asv_info exynos5420_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5420_asv_ops_arm,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5420_asv_ops_kfc,
		.asv_group_nr	= ASV_GRP_NR(KFC),
		.dvfs_level_nr	= DVFS_LEVEL_NR(KFC),
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos5420_asv_ops_int,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos5420_asv_ops_mif,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos5420_asv_ops_g3d,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	},
};

unsigned int exynos5420_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos5420_asv_member); i++)
		add_asv_member(&exynos5420_asv_member[i]);

	return 0;
}

int exynos5420_init_asv(struct asv_common *asv_info)
{
	struct clk *clk_abb;
	unsigned int chip_id3_value;
	unsigned int chip_id4_value;

	special_lot_group = 0;
	is_speedgroup = false;

	/* enable abb clock */
	clk_abb = clk_get(NULL, "clk_abb_apbif");
	if (IS_ERR(clk_abb)) {
		pr_err("EXYNOS5420 ASV : cannot find abb clock!\n");
		return -EINVAL;
	}
	clk_enable(clk_abb);

	chip_id3_value = __raw_readl(CHIP_ID3_REG);
	chip_id4_value = __raw_readl(CHIP_ID4_REG);

	if ((chip_id3_value >> EXYNOS5420_USESG_OFFSET) & EXYNOS5420_USESG_MASK) {
		if (!((chip_id3_value >> EXYNOS5420_SG_BSIGN_OFFSET) & EXYNOS5420_SG_BSIGN_MASK))
			special_lot_group = ((chip_id3_value >> EXYNOS5420_SG_A_OFFSET) & EXYNOS5420_SG_A_MASK)
					- ((chip_id3_value >> EXYNOS5420_SG_B_OFFSET) & EXYNOS5420_SG_B_MASK);
		else
			special_lot_group = ((chip_id3_value >> EXYNOS5420_SG_A_OFFSET) & EXYNOS5420_SG_A_MASK)
					+ ((chip_id3_value >> EXYNOS5420_SG_B_OFFSET) & EXYNOS5420_SG_B_MASK);
		is_speedgroup = true;
		asv_ref_info.is_speedgroup = true;
		pr_info("Exynos5420 ASV : Use Fusing Speed Group %d\n", special_lot_group);
	} else {
		asv_info->hpm_value = (chip_id4_value >> EXYNOS5420_TMCB_OFFSET) & EXYNOS5420_TMCB_MASK;
		asv_info->ids_value = (chip_id3_value >> EXYNOS5420_IDS_OFFSET) & EXYNOS5420_IDS_MASK;
		asv_ref_info.hpm_value = asv_info->hpm_value;
		asv_ref_info.ids_value = asv_info->ids_value;
	}

	if (!asv_info->hpm_value)
		pr_err("Exynos5420 ASV : invalid IDS value\n");

	pr_info("EXYNOS5420 ASV : %s IDS : %d HPM : %d\n", asv_info->lot_name,
				asv_info->ids_value, asv_info->hpm_value);

	asv_table_version = (chip_id3_value >> EXYNOS5420_TABLE_OFFSET) & EXYNOS5420_TABLE_MASK;
	asv_volt_offset[ID_ARM][0] = (chip_id4_value >> EXYNOS5420_EGLLOCK_UP_OFFSET) & EXYNOS5420_EGLLOCK_UP_MASK;
	asv_volt_offset[ID_ARM][1] = (chip_id4_value >> EXYNOS5420_EGLLOCK_DN_OFFSET) & EXYNOS5420_EGLLOCK_DN_MASK;
	asv_volt_offset[ID_KFC][0] = (chip_id4_value >> EXYNOS5420_KFCLOCK_UP_OFFSET) & EXYNOS5420_KFCLOCK_UP_MASK;
	asv_volt_offset[ID_KFC][1] = (chip_id4_value >> EXYNOS5420_KFCLOCK_DN_OFFSET) & EXYNOS5420_KFCLOCK_DN_MASK;
	asv_volt_offset[ID_INT][0] = (chip_id4_value >> EXYNOS5420_INTLOCK_UP_OFFSET) & EXYNOS5420_INTLOCK_UP_MASK;
	asv_volt_offset[ID_INT][1] = (chip_id4_value >> EXYNOS5420_INTLOCK_DN_OFFSET) & EXYNOS5420_INTLOCK_DN_MASK;
	asv_volt_offset[ID_G3D][0] = (chip_id4_value >> EXYNOS5420_G3DLOCK_UP_OFFSET) & EXYNOS5420_G3DLOCK_UP_MASK;
	asv_volt_offset[ID_G3D][1] = (chip_id4_value >> EXYNOS5420_G3DLOCK_DN_OFFSET) & EXYNOS5420_G3DLOCK_DN_MASK;
	asv_volt_offset[ID_MIF][0] = (chip_id4_value >> EXYNOS5420_MIFLOCK_UP_OFFSET) & EXYNOS5420_MIFLOCK_UP_MASK;
	asv_volt_offset[ID_MIF][1] = (chip_id4_value >> EXYNOS5420_MIFLOCK_DN_OFFSET) & EXYNOS5420_MIFLOCK_DN_MASK;

	asv_info->regist_asv_member = exynos5420_regist_asv_member;

	return 0;
}

static unsigned int exynos5420_get_asv_group_sram(void)
{
	unsigned int i;

	for (i = 0; i < ASV_GRP_NR(MIF); i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_ref_info.ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_ref_info.hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void set_ema(void)
{
	unsigned int ema0_val;
	unsigned int ema1_val;

	ema0_val = __raw_readl(S3C_VA_SYS+0x400);
	ema0_val = (ema0_val & ~(0x7 << 21)) | (0x5 << 21);
	__raw_writel(ema0_val, S3C_VA_SYS+0x400);

	ema1_val = __raw_readl(S3C_VA_SYS+0x404);
	ema1_val = (ema1_val & ~(0xfff << 9)) | (0x5 << 18) | (0x5 << 15) | (0x5 << 12) | (0x5 << 9);
	__raw_writel(ema1_val, S3C_VA_SYS+0x404);
}

static int __init asv_exynos5420_init(void)
{
	int asv_group_no;
	unsigned int mif_sram_volt;
	unsigned int g3d_sram_volt;

	struct regulator *mif_sram_regulator;
	struct regulator *g3d_sram_regulator;

	set_ema();

	mif_sram_regulator = regulator_get(NULL, "vdd_mifs");
	g3d_sram_regulator = regulator_get(NULL, "vdd_g3ds");

	asv_group_no = exynos5420_get_asv_group_sram();
	mif_sram_volt = mif_sram_asv_volt_info_evt1[0][asv_group_no];
	g3d_sram_volt = g3d_sram_asv_volt_info_evt1[0][asv_group_no];

	pr_info("SRAM ASV group [%d] : MIF(%d), G3D(%d)\n", asv_group_no, mif_sram_volt, g3d_sram_volt);

	if (!IS_ERR(mif_sram_regulator))
		regulator_set_voltage(mif_sram_regulator, mif_sram_volt, mif_sram_volt);
	else {
		pr_err("regulator get error : mif_sram\n");
		goto err_mif_sram;
	}

	if (!IS_ERR(g3d_sram_regulator))
		regulator_set_voltage(g3d_sram_regulator, g3d_sram_volt, g3d_sram_volt);
	else {
		pr_err("regulator get error : g3d_sram\n");
		goto err_g3d_sram;
	}

	regulator_put(g3d_sram_regulator);
err_g3d_sram:
	regulator_put(mif_sram_regulator);
err_mif_sram:

	return 0;
}

late_initcall(asv_exynos5420_init);
