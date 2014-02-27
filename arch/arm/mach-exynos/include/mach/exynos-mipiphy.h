/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MACH_MIPI_PHY_H
#define __MACH_MIPI_PHY_H __FILE__

extern int s5p_csis_phy_enable(int id, bool on);
extern int s5p_dsim_phy_enable(int id, bool on);

#endif /* __MACH_MIPI_PHY_H */
