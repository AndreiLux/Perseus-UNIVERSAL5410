/*
 *  chromeos_laptop.c - Driver to instantiate Chromebook platform devices.
 *
 *  Copyright (C) 2011 The Chromium OS Authors
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
#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/i2c/cyapa.h>
#include <linux/module.h>


/**************************************************************************/
/**                    Setup for LUMPY EVT board                         **/
/**************************************************************************/
#define LUMPY_TOUCHPAD_I2C_ADDR    0x67 /* I2C address (get from cyapa?) */

static struct i2c_client *lumpy_tp;
/**************************************************************************/


static int bus = 15;
module_param(bus, int, S_IRUGO);
MODULE_PARM_DESC(bus, "i2c bus number for peripheral bus");
static struct i2c_board_info __initdata cyapa_device = {
	I2C_BOARD_INFO(CYAPA_I2C_NAME, LUMPY_TOUCHPAD_I2C_ADDR),
	.irq		= -1,
};

static const struct dmi_system_id lumpy[] = {
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Lumpy"),
		},
	},
	{ }
};

static int lumpy_add_devices()
{
	const struct dmi_device *dmi_tp;
	struct dmi_dev_onboard *dev_data;
	int type = DMI_DEV_TYPE_DEV_ONBOARD;
	struct i2c_adapter *adapter;

	adapter = i2c_get_adapter(bus);
	if (!adapter) {
		pr_err("%s: i2c adapter bus %d invalid\n", __func__, bus);
		return -ENXIO;
	}

	/*  cyapa */
	dmi_tp = dmi_find_device(type, "trackpad", NULL);
	if (!dmi_tp) {
		pr_err("%s failed to dmi find device trackpad.\n",
		       __func__);
		return -ENODEV;
	}
	dev_data = (struct dmi_dev_onboard *)dmi_tp->device_data;
	if (!dev_data) {
		pr_err("%s failed to get device data from dmi.\n",
		       __func__);
		return -ENODEV;
	}
	cyapa_device.irq = dev_data->instance;
	lumpy_tp = i2c_new_device(adapter, &cyapa_device);
	if (!lumpy_tp) {
		pr_err("%s failed to register device %d-%02x\n", __func__,
		       bus, LUMPY_TOUCHPAD_I2C_ADDR);
		return -ENODEV;
	}
	i2c_put_adapter(adapter);
	return 0;
}


static int __init chromeos_laptop_init(void)
{
	if (!dmi_check_system(lumpy)) {
		pr_debug("%s unsupported system.\n", __func__);
		return -ENODEV;
	}
	return lumpy_add_devices();
}

static void __exit chromeos_laptop_exit(void)
{
	if (lumpy_tp) {
		i2c_unregister_device(lumpy_tp);
		lumpy_tp = NULL;
	}
}

module_init(chromeos_laptop_init);
module_exit(chromeos_laptop_exit);

MODULE_DESCRIPTION("Chrome OS Laptop");
MODULE_AUTHOR("bleung@chromium.org");
MODULE_LICENSE("GPL");
