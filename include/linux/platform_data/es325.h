/*
 * include/linux/platform_data/es325.h - Audience ES325 Voice Processor driver
 *
 * Copyright (C) 2012 Google, Inc.
 * Copyright (C) 2012 Samsung Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ES325_H__
#define __ES325_H__
#define PASSTHROUGH_NUM	2

struct es325_passthrough {
	int src;
	int dst;
};

struct es325_veq_parm {
	s16 noise_estimate_adj;
	s16 max_gain;
};

struct es325_platform_data {
	int gpio_wakeup;
	int gpio_reset;

	void (*clk_enable)(bool enable, bool force);
	char *clk_src;

	/* PORT A = 1, B = 2, C = 3, D = 4 */
	struct es325_passthrough pass[2];
	int passthrough_src;
	int passthrough_dst;
	bool use_sleep;

	struct es325_veq_parm *veq_parm_table;
	u8 veq_parm_table_size;
};

#endif
