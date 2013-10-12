/* include/linux/isa1200_vibrator.h
 *
 * Copyright (C) 2011 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ISA1200_H
#define _ISA1200_H

#define ISA1200_DEVICE_NAME		"isa1200"

#include <linux/timed_output.h>

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define ISA1200_DEBUG_LOG
#endif

#define ISA1200_CTRL0				0x30
#define ISA1200_CTRL1				0x31
#define ISA1200_CTRL2				0x32
#define ISA1200_CTRL3				0x33
#define ISA1200_CTRL4				0x34
#define ISA1200_DUTY				0x35
#define ISA1200_PERIOD				0x36
#define ISA1200_AMPLITUDE			0x37

/* ISA1200_CTRL0 */
#define CTL0_DIVIDER128				0
#define CTL0_DIVIDER256				1
#define CTL0_DIVIDER512				2
#define CTL0_DIVIDER1024			3
#define CTL0_13MHZ					1 << 2
#define CTL0_PWM_INPUT			1 << 3
#define CTL0_PWM_GEN				2 << 3
#define CTL0_WAVE_GEN				3 << 3
#define CTL0_HIGH_DRIVE			1 << 5
#define CTL0_OVER_DR_EN			1 << 6
#define CTL0_NORMAL_OP			1 << 7

/* ISA1200_CTRL1 */
#define CTL1_HAPTICOFF_16U			0
#define CTL1_HAPTICOFF_32U			1
#define CTL1_HAPTICOFF_64U			2
#define CTL1_HAPTICOFF_100U		3
#define CTL1_HAPTICON_1U			1 << 2
#define CTL1_SMART_EN				1 << 3
#define CTL1_PLL_EN					1 << 4
#define CTL1_ERM_TYPE				1 << 5
#define CTL1_DEFAULT				1 << 6
#define CTL1_EXT_CLOCK				1 << 7

/* ISA1200_CTRL2 */
#define CTL2_EFFECT_EN				1
#define CTL2_START_EFF_EN			1 << 2
#define CTL2_SOFT_RESET_EN			1 << 7

enum ISA1200_REG {
	ISA1200_REG_INIT = 0,
	ISA1200_REG_START,
	ISA1200_REG_STOP,
	ISA1200_REG_MAX,
};

struct isa1200_pdata {
	int (*gpio_en)(bool);
	int (*pwm_en)(bool);
	int (*pwm_cfg)(int) ;
	void (*pwm_init)(void);
	int max_timeout;
	const u8 **reg_data;
};

struct isa1200_drvdata {
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	struct i2c_client *client;
	struct isa1200_pdata *pdata;
	spinlock_t lock;
	bool running;
	int timeout;
	int max_timeout;
};

#if defined(CONFIG_VIBETONZ)
extern int vibtonz_i2c_write(u8 addr, int length, u8 *data);
extern void vibtonz_clk_enable(bool en);
extern void vibtonz_clk_config(int duty);
extern void vibtonz_chip_enable(bool en);
#endif

#endif	/* _ISA1200_H */
