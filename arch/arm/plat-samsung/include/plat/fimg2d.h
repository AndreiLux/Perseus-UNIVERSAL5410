/* linux/arch/arm/plat-s5p/include/plat/fimg2d.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Platform Data Structure for Samsung Graphics 2D Hardware
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_FIMG2D_H
#define __ASM_ARCH_FIMG2D_H __FILE__

#define FIMG2D_SET_CLK_NAME		"aclk_333_g2d_dout"

#if	defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ)
#define CONFIG_FIMG2D_USE_BUS_DEVFREQ
#endif

enum fimg2d_ip_version {
	IP_VER_G2D_4P,
	IP_VER_G2D_5G,
	IP_VER_G2D_5A,
	IP_VER_G2D_5AR,
};

struct fimg2d_platdata {
	int ip_ver;
	int hw_ver;
	const char *parent_clkname;
	const char *clkname;
	const char *gate_clkname;
	unsigned long clkrate;
	int  cpu_min;
	int  mif_min;
	int  int_min;
};

extern void __init s5p_fimg2d_set_platdata(struct fimg2d_platdata *pd);

#endif /* __ASM_ARCH_FIMG2D_H */
