/*
 * LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LP8556_H
#define _LP8556_H


struct lp8556_rom_data {
	u8 addr;
	u8 val;
};

/**
 * struct lp855x_platform_data
 * @name : Backlight driver name. If it is not defined, default name is set.
 * @load_new_rom_data :
	0 : use default configuration data
	1 : update values of eeprom or eprom registers on loading driver
 * @size_program : total size of lp855x_rom_data
 * @rom_data : list of new eeprom/eprom registers
 */
struct lp8556_platform_data {
	char *name;
	u8 load_new_rom_data;
	int size_program;
	struct lp8556_rom_data *rom_data;
};

#endif
