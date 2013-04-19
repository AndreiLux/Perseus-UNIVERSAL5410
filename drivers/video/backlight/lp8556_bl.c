/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/lp8556.h>

/* Registers */
#define BRIGHTNESS_CTRL		0x00
#define DEVICE_CTRL		    0x01

#define BUF_SIZE		    20

struct lp8556 {
	const char *chipname;
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	struct mutex xfer_lock;
	struct lp8556_platform_data *pdata;
};

struct lp8556 *g_lp8556;


static int lp8556_read_byte(struct lp8556 *lp, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&lp->xfer_lock);
	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		mutex_unlock(&lp->xfer_lock);
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}
	mutex_unlock(&lp->xfer_lock);

	*data = (u8)ret;
	return 0;
}

static int lp8556_write_byte(struct lp8556 *lp, u8 reg, u8 data)
{
	int ret;

	printk(KERN_INFO "LP8556 Write .. reg : 0x%02x, data : 0x%02x\n", reg, data);

	mutex_lock(&lp->xfer_lock);
	ret = i2c_smbus_write_byte_data(lp->client, reg, data);
	mutex_unlock(&lp->xfer_lock);

	return ret;
}

int lp8556_init_registers(void)
{
	u8 val, addr;
	int i, ret = 0;
	struct lp8556_platform_data *pd;

	if (!g_lp8556) {
		printk(KERN_ERR "[LP8556:ERR] lp8556 is null\n");
		return -EIO;
	}

	pd = g_lp8556->pdata;

	if (pd->load_new_rom_data && pd->size_program) {
		for (i = 0; i < pd->size_program; i++) {
			addr = pd->rom_data[i].addr;
			val = pd->rom_data[i].val;
			ret = lp8556_write_byte(g_lp8556, addr, val);
			if (ret)
				return ret;
		}
	}
	return ret;
}
EXPORT_SYMBOL(lp8556_init_registers);


static ssize_t lp8556_get_chip_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lp8556 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, BUF_SIZE, "%s\n", lp->chipname);
}

static DEVICE_ATTR(chip_id, S_IRUGO, lp8556_get_chip_id, NULL);



static ssize_t lp8556_dump(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	u8 data = 0;
	unsigned int reg;
	struct lp8556 *lp = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &reg);
	if (ret)
		return ret;

	printk(KERN_INFO "dump reg addr : 0x%02x\n", reg);

	ret = lp8556_read_byte(lp, (u8)reg, &data);
	if (ret)
		printk(KERN_ERR "[LP8556:ERR] : lp8556_read_byte failed : %d\n", ret);
	else
		printk(KERN_INFO "READ REG : 0x%02x, VALUE : 0x%02x\n", reg, data);

	return count;
}

static DEVICE_ATTR(dump, S_IWUSR, NULL, lp8556_dump);

static struct attribute *lp8556_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_dump.attr,
	NULL,
};

static const struct attribute_group lp8556_attr_group = {
	.attrs = lp8556_attributes,
};

static int lp8556_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lp8556 *lp;
	struct lp8556_platform_data *pdata = cl->dev.platform_data;
	int ret;

	if (!pdata) {
		dev_err(&cl->dev, "no platform data supplied\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp8556), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = cl;
	lp->dev = &cl->dev;
	lp->pdata = pdata;
	lp->chipname = id->name;
	i2c_set_clientdata(cl, lp);

	mutex_init(&lp->xfer_lock);

	ret = sysfs_create_group(&lp->dev->kobj, &lp8556_attr_group);
	if (ret) {
		dev_err(lp->dev, "failed to register sysfs. err: %d\n", ret);
		return ret;
	}

	g_lp8556 = lp;

	return 0;

}

static int __devexit lp8556_remove(struct i2c_client *cl)
{
	struct lp8556 *lp = i2c_get_clientdata(cl);

	sysfs_remove_group(&lp->dev->kobj, &lp8556_attr_group);
	return 0;
}

static const struct i2c_device_id lp8556_ids[] = {
	{"lp8556", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static struct i2c_driver lp8556_driver = {
	.driver = {
		.name = "lp8556",
		.owner = THIS_MODULE,
	},
	.probe = lp8556_probe,
	.remove = __devexit_p(lp8556_remove),
	.id_table = lp8556_ids,
};

module_i2c_driver(lp8556_driver);

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
