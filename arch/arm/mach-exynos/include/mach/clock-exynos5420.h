/*
 * Copyright (c) 2011-2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for exynos5420 clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#ifndef __ASM_ARCH_CLOCK_EXYNOS5420_H
#define __ASM_ARCH_CLOCK_EXYNOS5420_H __FILE__

/* clk control funtion for exynos5420 */
extern int exynos5420_bpll_set_rate(struct clk *clk, unsigned long rate);

#endif
