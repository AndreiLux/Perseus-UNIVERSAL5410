/*
 *  chromeos_laptop.c - Driver to instantiate Chromebook platform devices.
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

#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/module.h>

#define CYAPA_TP_I2C_ADDR	0x67
#define ISL_ALS_I2C_ADDR	0x44
#define TAOS_ALS_I2C_ADDR	0x29

static struct i2c_client *tp;
static struct i2c_client *als;

const char *i2c_adapter_names[] = {
	"SMBus I801 adapter",
	"i915 gmbus vga",
	"i915 gmbus panel",
};

enum i2c_adapter_type {
	I2C_ADAPTER_SMBUS = 0,
	I2C_ADAPTER_VGADDC,
	I2C_ADAPTER_PANEL,
};

static struct i2c_board_info __initdata cyapa_device = {
	I2C_BOARD_INFO("cyapa", CYAPA_TP_I2C_ADDR),
	.irq		= -1,
	.flags		= I2C_CLIENT_WAKE,
};

static struct i2c_board_info __initdata isl_als_device = {
	I2C_BOARD_INFO("isl29018", ISL_ALS_I2C_ADDR),
};

static struct i2c_board_info __initdata tsl2583_als_device = {
	I2C_BOARD_INFO("tsl2583", TAOS_ALS_I2C_ADDR),
};

static struct i2c_board_info __initdata tsl2563_als_device = {
	I2C_BOARD_INFO("tsl2563", TAOS_ALS_I2C_ADDR),
};

static struct i2c_client *__add_i2c_device(const char *name, int bus,
					   struct i2c_board_info *info)
{
	const struct dmi_device *dmi_dev;
	const struct dmi_dev_onboard *dev_data;
	struct i2c_adapter *adapter;
	struct i2c_client *client;

	if (bus < 0)
		return NULL;
	/*
	 * If a name is specified, look for irq platform information stashed
	 * in DMI_DEV_TYPE_DEV_ONBOARD.
	 */
	if (name) {
		dmi_dev = dmi_find_device(DMI_DEV_TYPE_DEV_ONBOARD, name, NULL);
		if (!dmi_dev) {
			pr_err("%s failed to dmi find device %s.\n",
			       __func__,
			       name);
			return NULL;
		}
		dev_data = (struct dmi_dev_onboard *)dmi_dev->device_data;
		if (!dev_data) {
			pr_err("%s failed to get data from dmi for %s.\n",
			       __func__, name);
			return NULL;
		}
		info->irq = dev_data->instance;
	}

	adapter = i2c_get_adapter(bus);

	/* add the i2c device */
	client = i2c_new_device(adapter, info);
	if (!client)
		pr_err("%s failed to register device %d-%02x\n",
		       __func__, bus, info->addr);
	else
		pr_debug("%s added i2c device %d-%02x\n",
			 __func__, bus, info->addr);

	i2c_put_adapter(adapter);
	return client;
}

static int __find_i2c_adap(struct device *dev, void *data)
{
	const char *name = data;
	const char *prefix = "i2c-";
	struct i2c_adapter *adapter;
	if (strncmp(dev_name(dev), prefix, strlen(prefix)))
		return 0;
	adapter = to_i2c_adapter(dev);
	return !strncmp(adapter->name, name, strlen(name));
}

static int find_i2c_adapter_num(enum i2c_adapter_type type)
{
	struct device *dev = NULL;
	struct i2c_adapter *adapter;
	/* find the SMBus adapter */
	dev = bus_find_device(&i2c_bus_type, NULL, i2c_adapter_names[type],
			      __find_i2c_adap);
	if (!dev) {
		pr_err("%s: i2c adapter %s not found on system.\n", __func__,
		       i2c_adapter_names[type]);
		return -ENODEV;
	}
	adapter = to_i2c_adapter(dev);
	return adapter->nr;
}

static struct i2c_client *add_i2c_device(const char *name,
					 enum i2c_adapter_type type,
					 struct i2c_board_info *info)
{
	return __add_i2c_device(name, find_i2c_adapter_num(type), info);
}

static struct i2c_client *add_smbus_device(const char *name,
					   struct i2c_board_info *info)
{
	return add_i2c_device(name, I2C_ADAPTER_SMBUS, info);
}

static int setup_cyapa_tp(const struct dmi_system_id *id)
{
	/* cyapa touchpad */
	tp = add_smbus_device("trackpad", &cyapa_device);
	return 0;
}

static int setup_isl29018_als(const struct dmi_system_id *id)
{
	/* add isl29018 light sensor */
	als = add_smbus_device("lightsensor", &isl_als_device);
	return 0;
}

static int setup_tsl2583_als(const struct dmi_system_id *id)
{
	/* add tsl2583 light sensor */
	als = add_smbus_device(NULL, &tsl2583_als_device);
	return 0;
}

static int setup_tsl2563_als(const struct dmi_system_id *id)
{
	/* add tsl2563 light sensor */
	als = add_smbus_device(NULL, &tsl2563_als_device);
	return 0;
}

static const struct dmi_system_id chromeos_laptop_dmi_table[] = {
	{
		.ident = "cyapa - Touchpad",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Lumpy"),
		},
		.callback = setup_cyapa_tp,
	},
	{
		.ident = "isl29018 - Light Sensor",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Link"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Lumpy"),
		},
		.callback = setup_isl29018_als,
	},
	{
		.ident = "tsl2583 - Light Sensor",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Alex"),
		},
		.callback = setup_tsl2583_als,
	},
	{
		.ident = "tsl2563 - Light Sensor",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Mario"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ZGB"),
		},
		.callback = setup_tsl2563_als,
	},
	{ }
};

static int __init chromeos_laptop_init(void)
{
	if (!dmi_check_system(chromeos_laptop_dmi_table)) {
		pr_debug("%s unsupported system.\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static void __exit chromeos_laptop_exit(void)
{
	if (tp) {
		i2c_unregister_device(tp);
		tp = NULL;
	}
	if (als) {
		i2c_unregister_device(als);
		als = NULL;
	}
}

module_init(chromeos_laptop_init);
module_exit(chromeos_laptop_exit);

MODULE_DESCRIPTION("Chrome OS Laptop");
MODULE_AUTHOR("bleung@chromium.org");
MODULE_LICENSE("GPL");
