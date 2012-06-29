/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef	__EXYNOS_BTS_H_
#define	__EXYNOS_BTS_H_

enum bts_priority {
	BTS_DISABLE,
	BTS_BE,
	BTS_HARDTIME,
};

enum bts_fbm_group {
	BTS_FBM_G0_L = (1<<1),
	BTS_FBM_G0_R = (1<<2),
	BTS_FBM_G1_L = (1<<3),
	BTS_FBM_G1_R = (1<<4),
	BTS_FBM_G2_L = (1<<5),
	BTS_FBM_G2_R = (1<<6),
};

enum bts_prior_change_action {
	BTS_ACT_NONE,
	BTS_ACT_OFF,
	BTS_ACT_CHANGE_FBM_PRIOR,
};

struct exynos_fbm_resource {
	enum bts_fbm_group fbm_group;
	enum bts_priority priority;
};

struct exynos_fbm_pdata {
	struct exynos_fbm_resource *res;
	int res_num;
};

struct exynos_bts_pdata {
	enum bts_priority def_priority;
	char *pd_name;
	char *clk_name;
	struct exynos_fbm_pdata *fbm;
	int res_num;
	bool changable_prior;
	enum bts_prior_change_action change_act;
};

/* BTS functions */
/* Enable BTS driver included in pd_block */
void exynos_bts_enable(char *pd_name);
/* Set priority on BTS drivers */
void exynos_bts_set_priority(struct device *dev, bool on);

#ifdef CONFIG_S5P_DEV_BTS
#define bts_enable(a) exynos_bts_enable((void *)a);
#define bts_set_priority(a, b) exynos_bts_set_priority(a, b);
#else
#define bts_enable(a) do {} while (0)
#define bts_set_priority(a, b) do {} while (0)
#endif
#endif	/* __EXYNOS_BTS_H_ */
