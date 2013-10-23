/*
 *  wacom_i2c_firm.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/wacom_i2c.h>

unsigned char *fw_data;
bool ums_binary;
extern unsigned int system_rev;

#if defined(CONFIG_MACH_V1)
const unsigned int fw_size = 0x0000;
const unsigned char mpu_type = 0x00;
unsigned int fw_ver_file = 0x0053;
unsigned char *fw_name = "epen/W9007_B878.bin";

char fw_chksum[] = { 0x1F, 0xA9, 0x12, 0x8F, 0x52, };
const char B804_chksum[] = { 0x1F, 0x53, 0x1C, 0xB3, 0x8B, };

#elif defined(CONFIG_MACH_HA)
const unsigned int fw_size = 0x0000;
const unsigned char mpu_type = 0x28;
unsigned int fw_ver_file = 0x1342;
unsigned char *fw_name = "epen/W9001_B911.bin";

char fw_chksum[] = { 0x1F, 0xAB, 0x8E, 0x93, 0x33, };
/*ver 1320*/
const char B887_chksum[] = { 0x1F, 0x6E, 0x2A, 0x13, 0x71, };
#endif

void wacom_i2c_set_firm_data(unsigned char *data)
{
	if (data == NULL) {
		fw_data = NULL;
		return;
	}

	fw_data = (unsigned char *)data;
	ums_binary = true;
}

/*Return digitizer type according to board rev*/
int wacom_i2c_get_digitizer_type(void)
{
#ifdef CONFIG_MACH_V1
	if (system_rev >= WACOM_DTYPE_B878_HWID)
		return EPEN_DTYPE_B878;
	else
		return EPEN_DTYPE_B804;
#elif defined(CONFIG_MACH_HA)
	if (system_rev >= WACOM_DTYPE_B911_HWID)
		return EPEN_DTYPE_B911;
	else
		return EPEN_DTYPE_B887;
#else
	return 0;
#endif
}

void wacom_i2c_init_firm_data(void)
{
	int type;
	type = wacom_i2c_get_digitizer_type();

#if defined(CONFIG_MACH_V1)
	if (type == EPEN_DTYPE_B878) {
		printk(KERN_DEBUG
			"epen:Digitizer type is B878\n");
	} else {
		printk(KERN_DEBUG
			"epen:Digitizer type is B804\n");
		fw_name = "epen/W9007_B804.bin";
		fw_ver_file = 0x0007;
		memcpy(fw_chksum, B804_chksum,
			sizeof(fw_chksum));
	}
#elif defined(CONFIG_MACH_HA)
	if (likely(type == EPEN_DTYPE_B911)) {
		printk(KERN_DEBUG
			"epen:Digitizer type is B911\n");
	} else if (type == EPEN_DTYPE_B887) {
		printk(KERN_DEBUG
			"epen:Digitizer type is B887\n");
		fw_name = "epen/W9001_B887.bin";
		fw_ver_file = 0x1320;
		memcpy(fw_chksum, B887_chksum,
			sizeof(fw_chksum));
	}
#endif
}
