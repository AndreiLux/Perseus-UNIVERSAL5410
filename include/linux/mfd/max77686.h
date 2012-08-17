/*
 * max77686.h - Driver for the Maxim 77686
 *
 *  Copyright (C) 2011 Samsung Electrnoics
 *  Chiwoong Byun <woong.byun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.h
 *
 * MAX77686 has PMIC, RTC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __LINUX_MFD_MAX77686_H
#define __LINUX_MFD_MAX77686_H

#include <linux/regulator/consumer.h>

/* MAX77686 regulator IDs */
enum max77686_regulators {
	MAX77686_LDO1 = 0,
	MAX77686_LDO2,
	MAX77686_LDO3,
	MAX77686_LDO4,
	MAX77686_LDO5,
	MAX77686_LDO6,
	MAX77686_LDO7,
	MAX77686_LDO8,
	MAX77686_LDO9,
	MAX77686_LDO10,
	MAX77686_LDO11,
	MAX77686_LDO12,
	MAX77686_LDO13,
	MAX77686_LDO14,
	MAX77686_LDO15,
	MAX77686_LDO16,
	MAX77686_LDO17,
	MAX77686_LDO18,
	MAX77686_LDO19,
	MAX77686_LDO20,
	MAX77686_LDO21,
	MAX77686_LDO22,
	MAX77686_LDO23,
	MAX77686_LDO24,
	MAX77686_LDO25,
	MAX77686_LDO26,
	MAX77686_BUCK1,
	MAX77686_BUCK2,
	MAX77686_BUCK3,
	MAX77686_BUCK4,
	MAX77686_BUCK5,
	MAX77686_BUCK6,
	MAX77686_BUCK7,
	MAX77686_BUCK8,
	MAX77686_BUCK9,
	MAX77686_EN32KHZ_AP,
	MAX77686_EN32KHZ_CP,
	MAX77686_P32KH,

	MAX77686_REG_MAX,
};

enum max77686_ramp_rate {
	MAX77686_RAMP_RATE_13MV = 0,
	MAX77686_RAMP_RATE_27MV,	/* default */
	MAX77686_RAMP_RATE_55MV,
	MAX77686_RAMP_RATE_100MV,
};

struct max77686_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
	unsigned int reg_op_mode;
};

struct max77686_platform_data {
	u8 ramp_delay;
	struct max77686_regulator_data *regulators;
	int num_regulators;
	struct max77686_opmode_data *opmode_data;

	/*
	 * GPIO-DVS feature is not enabled with the current version of
	 * MAX77686 driver. Buck2/3/4_voltages[0] is used as the default
	 * voltage at probe.
	 */
};


extern int max77686_debug_mask;		/* enables debug prints */

enum {
	MAX77686_DEBUG_INFO = 1 << 0,
	MAX77686_DEBUG_MASK = 1 << 1,
	MAX77686_DEBUG_INT = 1 << 2,
};

#ifndef CONFIG_DEBUG_MAX77686

#define dbg_mask(fmt, ...) do { } while (0)
#define dbg_info(fmt, ...) do { } while (0)
#define dbg_int(fmt, ...) do { } while (0)

#else

#define dbg_mask(fmt, ...)					\
do {								\
	if (max77686_debug_mask & MAX77686_DEBUG_MASK)		\
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__);	\
} while (0)

#define dbg_info(fmt, ...)					\
do {								\
	if (max77686_debug_mask & MAX77686_DEBUG_INFO)		\
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__);	\
} while (0)

#define dbg_int(fmt, ...)					\
do {								\
	if (max77686_debug_mask & MAX77686_DEBUG_INT)		\
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__);	\
} while (0)
#endif /* DEBUG_MAX77686 */

#endif /* __LINUX_MFD_MAX77686_H */
