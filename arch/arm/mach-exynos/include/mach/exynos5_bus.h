/*
 * Copyright (c) 2012 Google, Inc.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Gnu General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MACH_EXYNOS_EXYNOS5_BUS_H_
#define _MACH_EXYNOS_EXYNOS5_BUS_H_

struct exynos5_bus_mif_platform_data {
	unsigned long max_freq;
};

void exynos5_ppmu_trace(void);

#endif /* _MACH_EXYNOS_EXYNOS5_BUS_H_ */
