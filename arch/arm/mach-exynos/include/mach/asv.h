/* linux/arch/arm/mach-exynos/include/mach/asv.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4 - Adaptive Support Voltage Header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_ASV_H
#define __ASM_ARCH_ASV_H __FILE__

#define JUDGE_TABLE_END			NULL
#define EXYNOS4_LOOP_CNT		10

struct asv_judge_table {
	unsigned int hpm_limit; /* HPM value to decide group of target */
	unsigned int ids_limit; /* IDS value to decide group of target */
};

struct samsung_asv {
	unsigned int pkg_id;			/* fused value for chip */
	unsigned int ids_offset;		/* ids_offset of chip */
	unsigned int ids_mask;			/* ids_mask of chip */
	unsigned int hpm_result;		/* hpm value of chip */
	unsigned int ids_result;		/* ids value of chip */
	int (*check_vdd_arm)(void);		/* check vdd_arm value, this function is selectable */
	int (*pre_clock_init)(void);		/* clock init function to get hpm */
	int (*pre_clock_setup)(void);		/* clock setup function to get hpm */
	/* specific get ids function */
	int (*get_ids)(struct samsung_asv *asv_info);
	/* specific get hpm function */
	int (*get_hpm)(struct samsung_asv *asv_info);
	/* store into some repository to send result of asv */
	int (*store_result)(struct samsung_asv *asv_info);
};

extern void exynos4210_asv_init(struct samsung_asv *asv_info);

#endif /* __ASM_ARCH_ASV_H */
