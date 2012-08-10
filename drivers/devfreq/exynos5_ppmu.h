/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - PPMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DEVFREQ_EXYNOS5_PPMU_H
#define __DEVFREQ_EXYNOS5_PPMU_H __FILE__

enum exynos_ppmu_sets {
	PPMU_SET_DDR,
	PPMU_SET_RIGHT,
	PPMU_SET_CPU,
};

struct exynos5_ppmu_handle;

struct exynos5_ppmu_handle *exynos5_ppmu_poll(void (*callback)(int, void *),
		void *data, enum exynos_ppmu_sets filter);
void exynos5_ppmu_poll_off(struct exynos5_ppmu_handle *handle);
void exynos5_ppmu_poll_on(struct exynos5_ppmu_handle *handle);

#endif /* __DEVFREQ_EXYNOS5_PPMU_H */

