/* linux/arch/arm/mach-exynos/asv-5250.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5250 - ASV(Adaptive Supply Voltage) driver
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
#include <linux/string.h>

#include <mach/map.h>

#include <plat/cpu.h>

/* ASV function for Fused Chip */
#define IDS_ARM_OFFSET		24
#define IDS_ARM_MASK		0xFF
#define HPM_OFFSET		12
#define HPM_MASK		0x1F

#define FUSED_SG_OFFSET		3
#define ORIG_SG_OFFSET		17
#define ORIG_SG_MASK		0xF
#define MOD_SG_OFFSET		21
#define MOD_SG_MASK		0x7

#define DEFAULT_ASV_GROUP	1
#define DEFAULT_MIF_ASV_GROUP	0

#define CHIP_ID_REG		(S5P_VA_CHIPID + 0x4)
#define LOT_ID_REG		(S5P_VA_CHIPID + 0x14)

/* ASV choice table based on HPM and IDS values */
struct asv_table {
	unsigned int hpm_limit; /* HPM value to decide target group */
	unsigned int ids_limit; /* IDS value to decide target group */
};

struct samsung_asv {
	unsigned int package_id;	/* fused value for pakage */
	unsigned int hpm_result;	/* hpm value of chip */
	unsigned int ids_result;	/* ids value of chip */
	/* returns IDS value */
	int (*get_ids)(struct samsung_asv *asv_info);
	/* returns HPM value */
	int (*get_hpm)(struct samsung_asv *asv_info);
	/* store asv result for later use */
	int (*store_result)(struct samsung_asv *asv_info);
};

unsigned int exynos_result_of_asv;
unsigned int exynos_result_mif_asv;
bool exynos_lot_id;
bool exynos_lot_is_nzvpu;

/* ASV group voltage table */
#define CPUFREQ_LEVEL_END	(16)
static const unsigned int asv_voltage_special[CPUFREQ_LEVEL_END][11] = {
	/* ASV0 does not exist */
	/*     ASV1,   ASV2,    ASV3,    ASV4,    ASV5,    ASV6,    ASV7,    ASV8,    ASV9,   ASV10 */
	{ 0, 1300000, 1275000, 1287500, 1275000, 1275000, 1262500, 1250000, 1237500, 1225000, 1225000 },    /* L0 */
	{ 0, 1250000, 1237500, 1250000, 1237500, 1250000, 1237500, 1225000, 1212500, 1200000, 1200000 },    /* L1 */
	{ 0, 1225000, 1200000, 1212500, 1200000, 1212500, 1200000, 1187500, 1175000, 1175000, 1150000 },    /* L2 */
	{ 0, 1200000, 1175000, 1200000, 1175000, 1187500, 1175000, 1162500, 1150000, 1137500, 1125000 },    /* L3 */
	{ 0, 1150000, 1125000, 1150000, 1125000, 1137500, 1125000, 1112500, 1100000, 1087500, 1075000 },    /* L4 */
	{ 0, 1125000, 1112500, 1125000, 1112500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500 },    /* L5 */
	{ 0, 1100000, 1075000, 1100000, 1087500, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500 },    /* L6 */
	{ 0, 1075000, 1050000, 1062500, 1050000, 1062500, 1050000, 1050000, 1037500, 1025000, 1012500 },    /* L7 */
	{ 0, 1050000, 1025000, 1050000, 1037500, 1050000, 1037500, 1025000, 1012500, 1000000,  987500 },    /* L8 */
	{ 0, 1025000, 1012500, 1025000, 1012500, 1025000, 1012500, 1000000, 1000000,  987500,  975000 },    /* L9 */
	{ 0, 1012500, 1000000, 1012500, 1000000, 1012500, 1000000,  987500,  975000,  975000,  962500 },    /* L10 */
	{ 0, 1000000,  975000, 1000000,  975000, 1000000,  987500,  975000,  962500,  962500,  950000 },    /* L11 */
	{ 0,  975000,  962500,  975000,  962500,  975000,  962500,  950000,  937500,  925000,  925000 },    /* L12 */
	{ 0,  950000,  937500,  950000,  937500,  950000,  937500,  925000,  925000,  925000,  912500 },    /* L13 */
	{ 0,  937500,  925000,  937500,  925000,  937500,  925000,  912500,  912500,  900000,  900000 },    /* L14 */
	{ 0,  925000,  912500,  925000,  912500,  925000,  912500,  900000,  900000,  887500,  887500 },    /* L15 */
};

static const unsigned int asv_voltage[CPUFREQ_LEVEL_END][12] = {
	{ 1300000, 1275000, 1275000, 1262500, 1250000, 1225000, 1212500, 1200000, 1187500, 1175000, 1150000, 1125000 },    /* L0 */
	{ 1250000, 1225000, 1225000, 1212500, 1200000, 1187500, 1175000, 1162500, 1150000, 1137500, 1112500, 1100000 },    /* L1 */
	{ 1225000, 1187500, 1175000, 1162500, 1150000, 1137500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500 },    /* L2 */
	{ 1200000, 1125000, 1125000, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000 },    /* L3 */
	{ 1150000, 1100000, 1100000, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000 },    /* L4 */
	{ 1125000, 1075000, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  975000 },    /* L5 */
	{ 1100000, 1050000, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  925000 },    /* L6 */
	{ 1075000, 1037500, 1037500, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500 },    /* L7 */
	{ 1050000, 1025000, 1012500,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  912500,  900000 },    /* L8 */
	{ 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  900000,  900000 },    /* L9 */
	{ 1012500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000 },    /* L10 */
	{ 1000000,  962500,  950000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000 },    /* L11 */
	{  975000,  950000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500 },    /* L12 */
	{  950000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500 },    /* L13 */
	{  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  875000 },    /* L14 */
	{  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  875000,  875000 },    /* L15 */
};

/* Original ASV table had 10 levels */
static struct asv_table exynos5250_limit_orig[] = {
	/* HPM, IDS */
	{ 0, 0},		/* Reserved Group */
	{ 9, 7},
	{ 10, 9},
	{ 12, 11},
	{ 14, 14},
	{ 15, 17},
	{ 16, 20},
	{ 17, 23},
	{ 18, 27},
	{ 19, 30},
	{ 100, 100},
	{ 999, 999},		/* Reserved Group */
};

/* New ASV table has 12 levels */
static struct asv_table exynos5250_limit[] = {
	/* HPM, IDS */
	{ 6, 7},
	{ 8, 9},
	{ 9, 10},
	{ 10, 11},
	{ 12, 13},
	{ 13, 15},
	{ 14, 17},
	{ 16, 21},
	{ 17, 25},
	{ 19, 32},
	{ 20, 39},
	{ 100, 100},
	{ 999, 999},		/* Reserved Group */
};

/* For MIF busfreq support */
static struct asv_table exynos5250_mif_limit[] = {
	/* HPM, LOCK */
	{ 0, 0},		/* Reserved Group */
	{ 12, 100},
	{ 15, 112},
	{ 100, 512},
};

const unsigned int exynos5250_cpufreq_get_asv(unsigned int index)
{
	unsigned int asv_group = exynos_result_of_asv;
	if (exynos_lot_id)
		return asv_voltage_special[index][asv_group];
	else
		return asv_voltage[index][asv_group];
}

char *special_lot_id_list[] = {
	"NZVPU",
	"NZVR7",
};

/*
 * If lot id is "NZVPU" the ARM_IDS value needs to be modified
 */
static int exynos5250_check_lot_id(struct samsung_asv *asv_info)
{
	unsigned int lid_reg = 0;
	unsigned int rev_lid = 0;
	unsigned int i;
	unsigned int tmp;
	unsigned int wno;
	char lot_id[5];

	lid_reg = __raw_readl(LOT_ID_REG);

	for (i = 0; i < 32; i++) {
		tmp = (lid_reg >> i) & 0x1;
		rev_lid += tmp << (31 - i);
	}

	lot_id[0] = 'N';
	lid_reg = (rev_lid >> 11) & 0x1FFFFF;

	for (i = 4; i >= 1; i--) {
		tmp = lid_reg % 36;
		lid_reg /= 36;
		lot_id[i] = (tmp < 10) ? (tmp + '0') : ((tmp - 10) + 'A');
	}

	wno = (rev_lid >> 6) & 0x1f;

	/* NZVPU lot has incorrect IDS value */
	if ((!strncmp(lot_id, "NZVPU", ARRAY_SIZE(lot_id)))) {
		exynos_lot_is_nzvpu = true;
		printk(KERN_INFO "Exynos5250: Lot ID is %s and wafer number is %d\n",
				lot_id, wno);
		if (wno >= 2 && wno <= 6)
			asv_info->ids_result -= 16;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(special_lot_id_list); i++) {
		if (!strncmp(lot_id, special_lot_id_list[i],
						ARRAY_SIZE(lot_id))) {
			printk(KERN_INFO "Exynos5250: Lot ID is %s\n", lot_id);
			return 0;
		}
	}

	return -EINVAL;
}

static int exynos5250_asv_store_result(struct samsung_asv *asv_info)
{
	unsigned int i;

	if (!exynos5250_check_lot_id(asv_info))
		exynos_lot_id = true;

	/* New ASV table */
	if (!exynos_lot_id) {
		for (i = 0; i < ARRAY_SIZE(exynos5250_limit); i++) {
			if ((asv_info->ids_result <=
					exynos5250_limit[i].ids_limit) || \
			    (asv_info->hpm_result <=
					exynos5250_limit[i].hpm_limit)) {
				exynos_result_of_asv = i;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(exynos5250_mif_limit); i++) {
			if (asv_info->hpm_result <=
					exynos5250_mif_limit[i].hpm_limit) {
				exynos_result_mif_asv = i;
				break;
			}
		}
	/* Original ASV table */
	} else {
		for (i = 0; i < ARRAY_SIZE(exynos5250_limit_orig); i++) {
			if ((asv_info->ids_result <=
					exynos5250_limit_orig[i].ids_limit) || \
			    (asv_info->hpm_result <=
					exynos5250_limit_orig[i].hpm_limit)) {
				exynos_result_of_asv = i;
				exynos_result_mif_asv = i;
				break;
			}
		}
	}
	/*
	 * If ASV result value is lower than the default value then choose
	 * the default group (1)
	 */
	if (exynos_result_of_asv < DEFAULT_ASV_GROUP)
		exynos_result_of_asv = DEFAULT_ASV_GROUP;

	printk(KERN_INFO "EXYNOS5250: IDS:%d HPM:%d RESULT:%d MIF:%d\n",
		asv_info->ids_result, asv_info->hpm_result,
		exynos_result_of_asv, exynos_result_mif_asv);

	return 0;
}

static int exynos5250_asv_init(void)
{
	unsigned int tmp;
	unsigned int exynos_orig_sp;
	unsigned int exynos_mod_sp;
	int exynos_cal_asv;
	struct samsung_asv *exynos_asv;

	printk(KERN_INFO  "EXYNOS5250: Adaptive Support Voltage init\n");

	exynos_asv = kzalloc(sizeof(struct samsung_asv), GFP_KERNEL);
	if (!exynos_asv)
		return -ENOMEM;

	tmp = __raw_readl(CHIP_ID_REG);
	exynos_asv->package_id = tmp; /* Store PKG_ID */

	/* If Speed group is fused then retrieve it from there */
	if ((tmp >> FUSED_SG_OFFSET) & 0x1) {
		exynos_orig_sp = (tmp >> ORIG_SG_OFFSET) & ORIG_SG_MASK;
		exynos_mod_sp = (tmp >> MOD_SG_OFFSET) & MOD_SG_MASK;
		exynos_cal_asv = exynos_orig_sp - exynos_mod_sp;

		/*
		 * If There is no origin speed group,
		 * store 1 asv group into exynos_result_of_asv.
		 */
		if (!exynos_orig_sp) {
			printk(KERN_INFO "EXYNOS5250: No Origin speed Group\n");
			exynos_result_of_asv = DEFAULT_ASV_GROUP;
		} else {
			if (exynos_cal_asv < DEFAULT_ASV_GROUP) {
				exynos_result_of_asv = DEFAULT_ASV_GROUP;
				exynos_result_mif_asv = DEFAULT_ASV_GROUP;
			} else {
				exynos_result_of_asv = exynos_cal_asv;
				exynos_result_mif_asv = DEFAULT_MIF_ASV_GROUP;
			}
		}

		printk(KERN_INFO "EXYNOS5250: ORIG: %d MOD: %d RESULT: %d\n",
			exynos_orig_sp, exynos_mod_sp, exynos_result_of_asv);

		return 0;
	}

	exynos_asv->ids_result = (exynos_asv->package_id >> IDS_ARM_OFFSET) & IDS_ARM_MASK;
	exynos_asv->hpm_result = (exynos_asv->package_id >> HPM_OFFSET) & HPM_MASK;
	exynos5250_asv_store_result(exynos_asv);

	return 0;
}
device_initcall_sync(exynos5250_asv_init);
