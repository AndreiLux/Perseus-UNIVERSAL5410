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
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#define ATMEL_TP_I2C_ADDR	0x4b
#define CYAPA_TP_I2C_ADDR	0x67
#define ISL_ALS_I2C_ADDR	0x44
#define TAOS_ALS_I2C_ADDR	0x29

static struct i2c_client *als;
static struct i2c_client *tp;

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

static const u8 atmel_224e_tp_config_data[] = {
	/* MXT_GEN_COMMAND(6) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_GEN_POWERCONFIG(7) */
	0xff, 0xff, 0x32,
	/* MXT_GEN_ACQUIRE(8) */
	0x06, 0x00, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_TOUCH_MULTI(9) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x00, 0x02, 0x01, 0x00, 0x0a, 0x03, 0x03, 0x0a, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x20, 0x00, 0x37, 0x37, 0x00,
	/* MXT_TOUCH_KEYARRAY(15) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	/* MXT_SPT_COMMSCONFIG(18) */
	0x00, 0x00,
	/* MXT_SPT_GPIOPWM(19) */
	0x03, 0xDF, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_TOUCH_PROXIMITY(23) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_SELFTEST(25)  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_GRIPSUPPRESSION(40) */
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_TOUCHSUPPRESSION(42)  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_CTECONFIG(46) */
	0x00, 0x02, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_STYLUS(47) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCG_NOISESUPPRESSION(48) */
	0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static struct mxt_platform_data atmel_224e_tp_platform_data = {
	.x_line			= 18,
	.y_line			= 12,
	.x_size			= 102*20,
	.y_size			= 68*20,
	.blen			= 0x20,	/* Gain setting is in upper 4 bits */
	.threshold		= 0x19,
	.voltage		= 0,	/* 3.3V */
	.orient			= MXT_HORIZONTAL_FLIP,
	.irqflags		= IRQF_TRIGGER_FALLING,
	.config			= atmel_224e_tp_config_data,
	.config_length		= sizeof(atmel_224e_tp_config_data),
};

static struct i2c_board_info __initdata atmel_224e_tp_device = {
	I2C_BOARD_INFO("atmel_mxt_tp", ATMEL_TP_I2C_ADDR),
	.platform_data = &atmel_224e_tp_platform_data,
	.irq = -1,
	.flags		= I2C_CLIENT_WAKE,
};

static const u8 atmel_224s_tp_config_data[] = {
	/* MXT_GEN_COMMAND(6) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_GEN_POWERCONFIG(7) */
	0xff, 0xff, 0x32, 0x00,
	/* MXT_GEN_ACQUIRE(8) */
	0x1e, 0x00, 0x28, 0x28, 0x00, 0x00, 0x28, 0x28, 0x00, 0x00,
	/* MXT_TOUCH_MULTI(9) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x0f, 0x0f, 0x00, 0x00, 0x0a,
	0xa4, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_TOUCH_KEYARRAY(15) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	/* MXT_SPT_COMMSCONFIG(18) */
	0x00, 0x00,
	/* MXT_SPT_GPIOPWM(19) */
	0x03, 0xdf, 0x00, 0x20, 0x00, 0x00,
	/* MXT_TOUCH_PROXIMITY(23) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_SELFTEST(25)  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_GRIPSUPPRESSION(40) */
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_TOUCHSUPPRESSION(42)  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_CTECONFIG(46) */
	0x04, 0x02, 0x10, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00,
	/* MXT_PROCI_STYLUS(47) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
	/* MXT_PROCI_ADAPTIVETHRESHOLD(55) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_SHIELDLESS(56) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_EXTRATOUCHSCREENDATA(57) */
	0x00, 0x00, 0x00,
	/* MXT_SPT_TIMER(61-1) */
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_TIMER(61-2) */
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCG_NOISESUPPRESSION(62) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static struct mxt_platform_data atmel_224s_tp_platform_data = {
	.x_line			= 18,
	.y_line			= 12,
	.x_size			= 102*20,
	.y_size			= 68*20,
	.blen			= 0x80,	/* Gain setting is in upper 4 bits */
	.threshold		= 0x32,
	.voltage		= 0,	/* 3.3V */
	.orient			= MXT_VERTICAL_FLIP,
	.irqflags		= IRQF_TRIGGER_FALLING,
	.config			= atmel_224s_tp_config_data,
	.config_length		= sizeof(atmel_224s_tp_config_data),
};

static struct i2c_board_info __initdata atmel_224s_tp_device = {
	I2C_BOARD_INFO("atmel_mxt_tp", ATMEL_TP_I2C_ADDR),
	.platform_data = &atmel_224s_tp_platform_data,
	.irq = -1,
	.flags		= I2C_CLIENT_WAKE,
};

static struct i2c_client *__add_probed_i2c_device(const char *name, int bus,
						  struct i2c_board_info *info,
						  const unsigned short *addrs)
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
	client = i2c_new_probed_device(adapter, info, addrs, NULL);
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
	const char *name = i2c_adapter_names[type];
	/* find the adapter by name */
	dev = bus_find_device(&i2c_bus_type, NULL, (void *)name,
			      __find_i2c_adap);
	if (!dev) {
		pr_err("%s: i2c adapter %s not found on system.\n", __func__,
		       name);
		return -ENODEV;
	}
	adapter = to_i2c_adapter(dev);
	return adapter->nr;
}

static struct i2c_client *add_probed_i2c_device(const char *name,
						enum i2c_adapter_type type,
						struct i2c_board_info *info,
						const unsigned short *addr_list)
{
	return __add_probed_i2c_device(name,
				       find_i2c_adapter_num(type),
				       info,
				       addr_list);
}

static struct i2c_client *add_i2c_device(const char *name,
					 enum i2c_adapter_type type,
					 struct i2c_board_info *info)
{
	const unsigned short addr_list[] = { info->addr, I2C_CLIENT_END };
	return __add_probed_i2c_device(name,
				       find_i2c_adapter_num(type),
				       info,
				       addr_list);
}

static struct i2c_client *add_smbus_device(const char *name,
					   struct i2c_board_info *info)
{
	return add_i2c_device(name, I2C_ADAPTER_SMBUS, info);
}

static int setup_link_tp(const struct dmi_system_id *id)
{
	/* first try cyapa touchpad */
	tp = add_i2c_device("trackpad", I2C_ADAPTER_VGADDC, &cyapa_device);
	if (tp)
		return 0;

	/* then try atmel mxt touchpad */
	tp = add_i2c_device("trackpad", I2C_ADAPTER_VGADDC,
			    &atmel_224s_tp_device);
	return 0;
}

static int setup_lumpy_tp(const struct dmi_system_id *id)
{
	/* first try cyapa touchpad on smbus */
	tp = add_smbus_device("trackpad", &cyapa_device);
	if (tp)
		return 0;

	/* then try atmel mxt touchpad */
	tp = add_i2c_device("trackpad", I2C_ADAPTER_VGADDC,
			    &atmel_224e_tp_device);
	return 0;
}

static int setup_isl29018_als(const struct dmi_system_id *id)
{
	/* add isl29018 light sensor */
	als = add_smbus_device("lightsensor", &isl_als_device);
	return 0;
}

static int setup_isl29023_als(const struct dmi_system_id *id)
{
	/* add isl29023 light sensor on Panel DDC GMBus */
	als = add_i2c_device("lightsensor", I2C_ADAPTER_PANEL, &isl_als_device);
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
		.ident = "Lumpy - Touchpads",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Lumpy"),
		},
		.callback = setup_lumpy_tp,
	},
	{
		.ident = "Link - Touchpads",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Link"),
		},
		.callback = setup_link_tp,
	},
	{
		.ident = "isl29018 - Light Sensor",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Lumpy"),
		},
		.callback = setup_isl29018_als,
	},
	{
		.ident = "isl29023 - Light Sensor",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Link"),
		},
		.callback = setup_isl29023_als,
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
		},
		.callback = setup_tsl2563_als,
	},
	{
		.ident = "tsl2563 - Light Sensor",
		.matches = {
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
	if (als) {
		i2c_unregister_device(als);
		als = NULL;
	}
	if (tp) {
		i2c_unregister_device(tp);
		tp = NULL;
	}
}

module_init(chromeos_laptop_init);
module_exit(chromeos_laptop_exit);

MODULE_DESCRIPTION("Chrome OS Laptop");
MODULE_AUTHOR("bleung@chromium.org");
MODULE_LICENSE("GPL");
