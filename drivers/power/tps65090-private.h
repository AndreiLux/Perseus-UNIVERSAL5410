/*
 * Core driver interface for TI TPS65090 PMIC family
 *
 * Copyright (c) 2012 Google, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __LINUX_MFD_TPS65090_PRIVATE_H
#define __LINUX_MFD_TPS65090_PRIVATE_H

#include <linux/i2c.h>

#define TPS65090_REG_INVALID	0xff

enum tps65090_pmic_reg {
	TPS65090_REG_IRQ1	= 0x00,
	TPS65090_REG_IRQ2	= 0x01,
	TPS65090_REG_CG_CTRL0	= 0x04,
	TPS65090_REG_CG_STATUS1	= 0x0a,
};

enum {
	TPS65090_IRQ1_IRQ_MASK		= 1 << 0,
	TPS65090_IRQ1_VACG_MASK		= 1 << 1,
	TPS65090_IRQ1_VSYSG_MASK	= 1 << 2,
	TPS65090_IRQ1_VBATG_MASK	= 1 << 3,
	TPS65090_IRQ1_CGACT_MASK	= 1 << 4,
	TPS65090_IRQ1_CGCPL_MASK	= 1 << 5,
};

#endif /*__LINUX_MFD_TPS65090_PRIVATE_H */
