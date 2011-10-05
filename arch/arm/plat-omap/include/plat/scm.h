/*
 * OMAP system control module device header file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: J Keerthy <j-keerthy@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_SCM_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_SCM_H

/**
 * struct omap4plus_scm_dev_attr - device attributes for scm
 * @rev: Revision id of OMAP Temperature sensor
 * @cnt: Number of temperature sensors.
 */
struct omap4plus_scm_dev_attr {
	int rev;
	int cnt;
};

/**
 * struct omap4plus_scm_pdata - omap4460plus_scm platform data
 * @rev: Revision id of OMAP Temperature sensor
 * @cnt: Number of temperature sensors.
 * @accurate: The temp sensor is accurate or not
 */
struct omap4plus_scm_pdata {
	int rev;
	int cnt;
	bool accurate;
};

#endif
