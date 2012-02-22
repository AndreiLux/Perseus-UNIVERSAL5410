/*
 * OMAP Voltage Management Routines
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Lesly A M <leslyam@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_VOLTAGE_COMMON_H
#define __ARCH_ARM_MACH_OMAP2_VOLTAGE_COMMON_H

/**
 * struct omap_volt_data - Omap voltage specific data.
 * @voltage_nominal:	The possible voltage value in uV
 * @sr_efuse_offs:	The offset of the efuse register(from system
 *			control module base address) from where to read
 *			the n-target value for the SVT SR sensors.
 * @lvt_sr_efuse_offs:	The offset of the efuse registers(from system
 *			control module base address) from where to read
 *			the n-target value for the LVT SR sencors.
 * @sr_errminlimit:	Error min limit value for smartreflex. This value
 *			differs at differnet opp and thus is linked
 *			with voltage.
 * @vp_errorgain:	Error gain value for the voltage processor. This
 *			field also differs according to the voltage/opp.
 */
struct omap_volt_data {
	u32	volt_nominal;
	u32	sr_efuse_offs;
	u32     lvt_sr_efuse_offs;
	u8	sr_errminlimit;
	u8	vp_errgain;
};

#endif
