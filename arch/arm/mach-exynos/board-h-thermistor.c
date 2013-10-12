/* board-h-thermistor.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>

static struct sec_therm_adc_table temper_table_ap[] = {
	{181, 800},
	{189, 790},
	{197, 780},
	{205, 770},
	{213, 760},
	{221, 750},
	{229, 740},
	{237, 730},
	{245, 720},
	{253, 710},
	{261, 700},
	{269, 690},
	{277, 680},
	{286, 670},
	{297, 660},
	{309, 650},
	{318, 640},
	{329, 630},
	{340, 620},
	{350, 610},
	{361, 600},
	{370, 590},
	{380, 580},
	{393, 570},
	{405, 560},
	{418, 550},
	{431, 540},
	{446, 530},
	{460, 520},
	{473, 510},
	{489, 500},
	{503, 490},
	{520, 480},
	{536, 470},
	{552, 460},
	{569, 450},
	{586, 440},
	{604, 430},
	{622, 420},
	{640, 410},
	{659, 400},
	{679, 390},
	{698, 380},
	{718, 370},
	{739, 360},
	{759, 350},
	{781, 340},
	{803, 330},
	{824, 320},
	{847, 310},
	{869, 300},
	{891, 290},
	{914, 280},
	{938, 270},
	{959, 260},
	{982, 250},
	{1005, 240},
	{1030, 230},
	{1052, 220},
	{1076, 210},
	{1099, 200},
	{1126, 190},
	{1145, 180},
	{1168, 170},
	{1193, 160},
	{1215, 150},
	{1239, 140},
	{1263, 130},
	{1286, 120},
	{1309, 110},
	{1333, 100},
	{1354, 90},
	{1377, 80},
	{1397, 70},
	{1418, 60},
	{1439, 50},
	{1459, 40},
	{1483, 30},
	{1497, 20},
	{1518, 10},
	{1537, 0},
	{1557, -10},
	{1577, -20},
	{1593, -30},
	{1611, -40},
	{1628, -50},
	{1645, -60},
	{1661, -70},
	{1678, -80},
	{1693, -90},
	{1704, -100},
	{1718, -110},
	{1732, -120},
	{1745, -130},
	{1757, -140},
	{1770, -150},
	{1781, -160},
	{1794, -170},
	{1805, -180},
	{1815, -190},
	{1827, -200},
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

void __init board_h_thermistor_init(void)
{
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
}
