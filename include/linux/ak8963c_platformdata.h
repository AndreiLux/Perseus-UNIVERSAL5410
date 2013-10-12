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

#ifndef _AK8963C_PLATFORMDATA_H_
#define _AK8963C_PLATFORMDATA_H_

#define AK8963C_TOP_LOWER_RIGHT         0
#define AK8963C_TOP_LOWER_LEFT          1
#define AK8963C_TOP_UPPER_LEFT          2
#define AK8963C_TOP_UPPER_RIGHT         3
#define AK8963C_BOTTOM_LOWER_RIGHT      4
#define AK8963C_BOTTOM_LOWER_LEFT       5
#define AK8963C_BOTTOM_UPPER_LEFT       6
#define AK8963C_BOTTOM_UPPER_RIGHT      7

struct ak8963c_platform_data {
	void (*get_pos)(int *);
	int m_rst_n;
	int m_sensor_int;
};
#endif
