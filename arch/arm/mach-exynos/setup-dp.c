/* linux/arch/arm/mach-exynos/setup-dp.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Base Samsung Exynos DP configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <mach/regs-clock.h>

void s5p_dp_phy_init(void)
{
	u32 reg;

	/* Reference clock selection for DPTX_PHY: PAD_OSC_IN */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg &= ~(1 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* Select clock source for DPTX_PHY as XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg &= ~(1 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);

	reg = __raw_readl(S5P_DPTX_PHY_CONTROL);
	reg |= S5P_DPTX_PHY_ENABLE;
	__raw_writel(reg, S5P_DPTX_PHY_CONTROL);
}

void s5p_dp_phy_exit(void)
{
	u32 reg;

	reg = __raw_readl(S5P_DPTX_PHY_CONTROL);
	reg &= ~S5P_DPTX_PHY_ENABLE;
	__raw_writel(reg, S5P_DPTX_PHY_CONTROL);
}
