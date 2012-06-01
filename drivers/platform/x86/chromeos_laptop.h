/*
 *  chromeos_laptop.h - Driver to instantiate Chromebook platform devices.
 *
 *  Copyright (C) 2012 The Chromium OS Authors
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* Keep this enum consistent with i2c_adapter_names in chromeos_laptop.c */
enum i2c_adapter_type {
	I2C_ADAPTER_SMBUS = 0,
	I2C_ADAPTER_VGADDC,
	I2C_ADAPTER_PANEL,
};

/*
 * Takes a list of addresses in addrs as such :
 * { addr1, ... , addrn, I2C_CLIENT_END };
 * chromeos_laptop_add_probed_i2c_device will use i2c_new_probed_device
 * and probe for devices at all of the addresses listed.
 * Returns NULL if no devices found.
 * See Documentation/i2c/instantiating-devices for more information.
 */
struct i2c_client *chromeos_laptop_add_probed_i2c_device(
		const char *name,
		enum i2c_adapter_type type,
		struct i2c_board_info *info,
		const unsigned short *addrs);

/*
 * Probes for a device at a single address, the one provided by
 * info->addr.
 * Returns NULL if no device found.
 */
struct i2c_client *chromeos_laptop_add_i2c_device(const char *name,
						  enum i2c_adapter_type type,
						  struct i2c_board_info *info);
