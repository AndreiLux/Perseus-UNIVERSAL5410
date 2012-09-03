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

void exynos5_ppmu_trace(void);
void exynos5_busfreq_mif_request_voltage_offset(unsigned long offset);

#if defined(CONFIG_ARM_EXYNOS5_BUS_DEVFREQ)
#define busfreq_mif_request_voltage_offset(a)  \
		exynos5_busfreq_mif_request_voltage_offset(a);
#else
#define busfreq_mif_request_voltage_offset(a)  do {} while (0)
#endif

#endif /* _MACH_EXYNOS_EXYNOS5_BUS_H_ */
