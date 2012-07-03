/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5250 - ASV(Adaptive Support Voltage) driver
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
#include <mach/asv-exynos5250.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/abb-exynos.h>

#include <plat/cpu.h>

/* ASV function for Fused Chip */
#define IDS_ARM_OFFSET		24
#define IDS_ARM_MASK		0xFF
#define HPM_OFFSET		12
#define HPM_MASK		0x1F

#define CHIP_ID_REG		(S5P_VA_CHIPID + 0x4)

static unsigned int asv_group[ID_END];

static unsigned int exynos5250_default_asv_max_volt[] = {
	[ID_ARM] = 1300000,
	[ID_INT] = 1037500,
	[ID_MIF] = 1125000,
	[ID_G3D] = 1200000,
};

static unsigned int asv_group_nr[] = {
	[ID_ARM] = ARM_ASV_GRP_NR,
	[ID_INT] = INT_ASV_GRP_NR,
	[ID_MIF] = MIF_ASV_GRP_NR,
	[ID_G3D] = G3D_ASV_GRP_NR,
};

static unsigned int dvfs_level_nr[] = {
	[ID_ARM] = ARM_DVFS_LEVEL_NR,
	[ID_INT] = INT_DVFS_LEVEL_NR,
	[ID_MIF] = MIF_DVFS_LEVEL_NR,
	[ID_G3D] = G3D_DVFS_LEVEL_NR,
};

typedef unsigned int (*refer_table_get_asv)[MAX_ASV_GRP_NR];

refer_table_get_asv refer_table[] = {
	[ID_ARM] = arm_refer_table_get_asv,
	[ID_INT] = int_refer_table_get_asv,
	[ID_MIF] = mif_refer_table_get_asv,
	[ID_G3D] = g3d_refer_table_get_asv,
};

typedef unsigned int (*asv_volt_info)[MAX_ASV_GRP_NR + 1];

asv_volt_info volt_table[] = {
	[ID_ARM] = arm_asv_volt_info,
	[ID_INT] = int_asv_volt_info,
	[ID_MIF] = mif_asv_volt_info,
	[ID_G3D] = g3d_asv_volt_info,
};

static void exynos5250_pre_set_abb(unsigned int asv_group_number)
{
	switch (asv_group_number) {
	case 0:
	case 1:
		set_abb_member(ABB_ARM, ABB_MODE_080V);
		set_abb_member(ABB_INT, ABB_MODE_080V);
		set_abb_member(ABB_G3D, ABB_MODE_080V);
		break;
	default:
		set_abb_member(ABB_ARM, ABB_MODE_BYPASS);
		set_abb_member(ABB_INT, ABB_MODE_BYPASS);
		set_abb_member(ABB_G3D, ABB_MODE_BYPASS);
		break;
	}

	set_abb_member(ABB_MIF, ABB_MODE_130V);
}
static unsigned int exynos5250_get_asv_group(unsigned int ids,
			unsigned int hpm, enum asv_type_id target_type)
{
	unsigned int i;
	unsigned int refer_ids;
	unsigned int refer_hpm;

	for (i = 0; i < asv_group_nr[target_type]; i++) {
		if (target_type != ID_MIF) {
			refer_ids = refer_table[target_type][0][i];
			refer_hpm = refer_table[target_type][1][i];

			if ((ids <= refer_ids) || (hpm <= refer_hpm))
				return i;
		} else {
			refer_hpm = refer_table[target_type][1][i];

			if (hpm <= refer_hpm)
				return i;
		}
	}

	/* Default max asv group */
	return 0;
}

unsigned int exynos5250_get_volt(enum asv_type_id target_type, unsigned int target_freq)
{
	int i;
	unsigned int group = asv_group[target_type];

	for (i = 0; i < dvfs_level_nr[target_type]; i++) {
		if (volt_table[target_type][i][0] == target_freq)
			return volt_table[target_type][i][group + 1];
	}

	return exynos5250_default_asv_max_volt[target_type];
}

int exynos5250_init_asv(struct asv_common *asv_info)
{
	int i;
	unsigned int tmp;
	unsigned hpm_value, ids_value;

	/* read IDS and HPM value from  CHIP ID */
	tmp = __raw_readl(CHIP_ID_REG);

	hpm_value = (tmp >> HPM_OFFSET) & HPM_MASK;
	ids_value = (tmp >> IDS_ARM_OFFSET) & IDS_ARM_MASK;

	pr_info("EXYNOS5250 IDS : %d HPM : %d\n",
		ids_value, hpm_value);

	for (i = ID_ARM; i < ID_END; i++)
		asv_group[i] = exynos5250_get_asv_group(ids_value, hpm_value, i);

	exynos5250_pre_set_abb(asv_group[ID_ARM]);

	asv_info->get_voltage = exynos5250_get_volt;
	asv_info->init_done = true;

	return 0;
}
