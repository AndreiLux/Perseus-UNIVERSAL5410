/*
 * board-universal_5410-thermistor.c - AP thermistor of Adonis Project
 *
 * Copyright (C) 2012 Samsung Electrnoics
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
 */

#include <linux/platform_device.h>

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#endif

#ifdef CONFIG_SEC_THERMISTOR
static struct sec_therm_adc_table temper_table_ap[] = {
	{242,	700},
	{251,	690},
	{261,	680},
	{268,	670},
	{285,	660},
	{287,	650},
	{289,	640},
	{297,	630},
	{305,	620},
	{316,	610},
	{327,	600},
	{339,	590},
	{351,	580},
	{364,	570},
	{376,	560},
	{388,	550},
	{401,	540},
	{414,	530},
	{427,	520},
	{440,	510},
	{452,	500},
	{469,	490},
	{486,	480},
	{503,	470},
	{520,	460},
	{536,	450},
	{556,	440},
	{577,	430},
	{597,	420},
	{618,	410},
	{638,	400},
	{653,	390},
	{668,	380},
	{683,	370},
	{698,	360},
	{713,	350},
	{731,	340},
	{749,	330},
	{767,	320},
	{787,	310},
	{805,	300},
	{815,	290},
	{825,	280},
	{848,	270},
	{868,	260},
	{886,	250},
	{906,	240},
	{926,	230},
	{949,	220},
	{955,	210},
	{981,	200},
	{1010,	190},
	{1040,	180},
	{1065,	170},
	{1093,	160},
	{1110,	150},
	{1130,	140},
	{1150,	130},
	{1172,	120},
	{1186,	110},
	{1204,	100},
	{1220,	90},
	{1240,	80},
	{1270,	70},
	{1300,	60},
	{1325,	50},
	{1356,	40},
	{1380,	30},
	{1408,	20},
	{1420,	10},
	{1440,	0},
	{1465,	-10},
	{1499,	-20},
	{1515,	-30},
	{1534,	-40},
	{1549,	-50},
	{1565,	-60},
	{1580,	-70},
	{1600,	-80},
	{1620,	-90},
	{1640,	-100},
	{1658,	-110},
	{1678,	-120},
	{1690,	-130},
	{1703,	-140},
	{1716,	-150},
	{1731,	-160},
	{1746,	-170},
	{1764,	-180},
	{1780,	-190},
	{1789,	-200},
};

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_channel	= 0,
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
};

struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};
#endif
