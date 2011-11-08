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
#include <linux/module.h>

#define LUMPY_TOUCHPAD_I2C_ADDR    0x67 /* I2C address */
#define LUMPY_ALS_I2C_ADDR         0x44
static struct i2c_client *lumpy_tp;
static struct i2c_client *lumpy_als;

static struct i2c_board_info __initdata cyapa_device = {
	I2C_BOARD_INFO("cyapa", LUMPY_TOUCHPAD_I2C_ADDR),
	.irq		= -1,
};

static struct i2c_board_info __initdata als_device = {
	I2C_BOARD_INFO("isl29018", LUMPY_ALS_I2C_ADDR),
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

static int __find_i2c_adap(struct device *dev, void *data)
{
	const char *name = data;
	struct i2c_adapter *adapter;
	if (strncmp(dev_name(dev), "i2c-", 4))
		return 0;
	adapter = to_i2c_adapter(dev);
	return !strncmp(adapter->name, name, strlen(name));
}

static struct i2c_client *add_smbus_device(struct i2c_adapter *adap,
					   const char *name,
					   struct i2c_board_info *info)
{
	const struct dmi_device *dmi_dev;
	const struct dmi_dev_onboard *dev_data;

	dmi_dev = dmi_find_device(DMI_DEV_TYPE_DEV_ONBOARD, name, NULL);
	if (!dmi_dev) {
		pr_err("%s failed to dmi find device %s.\n", __func__, name);
		return NULL;
	}
	dev_data = (struct dmi_dev_onboard *)dmi_dev->device_data;
	if (!dev_data) {
		pr_err("%s failed to get device data from dmi.\n",
		       __func__);
		return NULL;
	}
	info->irq = dev_data->instance;
	return i2c_new_device(adap, info);
}

static int lumpy_add_devices(void)
{
	struct i2c_adapter *adapter;
	struct device *dev = NULL;
	int bus;

	/* find the SMBus adapter */
	dev = bus_find_device(&i2c_bus_type, NULL, "SMBus I801 adapter",
			      __find_i2c_adap);
	if (!dev) {
		pr_err("%s: no SMBus adapter found on system.\n", __func__);
		return -ENXIO;
	}
	adapter = to_i2c_adapter(dev);
	bus = adapter->nr;

	/*  add cyapa */
	lumpy_tp = add_smbus_device(adapter, "trackpad", &cyapa_device);
	if (!lumpy_tp)
		pr_err("%s failed to register device %d-%02x\n",
		       __func__, bus, LUMPY_TOUCHPAD_I2C_ADDR);

	/* add isl light sensor */
	lumpy_als = add_smbus_device(adapter, "lightsensor", &als_device);
	if (!lumpy_als)
		pr_err("%s failed to register device %d-%02x\n",
		       __func__, bus, LUMPY_ALS_I2C_ADDR);

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
	if (lumpy_als) {
		i2c_unregister_device(lumpy_als);
		lumpy_als = NULL;
	}
}

module_init(chromeos_laptop_init);
module_exit(chromeos_laptop_exit);

MODULE_DESCRIPTION("Chrome OS Laptop");
MODULE_AUTHOR("bleung@chromium.org");
MODULE_LICENSE("GPL");
