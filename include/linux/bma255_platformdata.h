/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 */

#ifndef _BMA255_PLATFORMDATA_H_
#define _BMA255_PLATFORMDATA_H_

#define BMA255_TOP_UPPER_RIGHT          0
#define BMA255_TOP_LOWER_RIGHT          1
#define BMA255_TOP_LOWER_LEFT           2
#define BMA255_TOP_UPPER_LEFT           3
#define BMA255_BOTTOM_UPPER_RIGHT       4
#define BMA255_BOTTOM_LOWER_RIGHT       5
#define BMA255_BOTTOM_LOWER_LEFT        6
#define BMA255_BOTTOM_UPPER_LEFT        7

struct bma255_platform_data {
	void (*get_pos)(int *);
	int acc_int1;
	int acc_int2;
};
#endif
