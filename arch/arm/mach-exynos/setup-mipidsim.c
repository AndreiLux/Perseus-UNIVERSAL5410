/* linux/arch/arm/mach-exynos/setup-mipidsim.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * ERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * A 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include <mach/exynos-mipiphy.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#include <plat/dsim.h>
#include <plat/clock.h>
#include <plat/regs-mipidsim.h>

int s5p_dsim_init_d_phy(struct mipi_dsim_device *dsim, unsigned int enable)
{
	/**
	 * DPHY and aster block must be enabled at the system initialization
	 * step before data access from/to DPHY begins.
	 */
	struct platform_device *pdev = to_platform_device(dsim->dev);
	s5p_dsim_phy_enable(pdev->id, enable);

	return 0;
}
