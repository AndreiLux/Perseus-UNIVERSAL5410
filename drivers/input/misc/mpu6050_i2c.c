/*
 * Implements I2C interface driver for INVENSENSE MPU6050
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Kishore Kadiyala <kishore.kadiyala@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/delay.h>

#include "mpu6050x.h"
#include <linux/input/mpu6050.h>

/**
 * mpu6050_i2c_write - Writes to register in MPU6050
 * @addr: I2c chip address
 * @reg: register address
 * @val: byte to be written
 * @msg: message string
 *
 * Returns '0' on success.
 */
static int mpu6050_i2c_write(struct device *dev, u8 addr, u8 reg,
						u8 val, u8 *msg)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_adapter *i2c_adap = client->adapter;
	struct i2c_msg msgs[1];
	int ret;
	u8 m[2] = {reg, val};

	if (i2c_adap == NULL)
		return -EINVAL;
	mdelay(1);

	msgs[0].addr = addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = m;
	msgs[0].len = 2;

	ret = i2c_transfer(i2c_adap, msgs, 1);
	if (ret < 1) {
		dev_err(&client->dev,
			"%s failed (%s, %d)\n", __func__, msg, ret);
		return ret;
	} else {
		return 0;
	}
}

/**
 * mpu6050_i2c_read - Read from register in MPU6050
 * @addr: I2c chip address
 * @reg: register address
 * @len: number of bytes to transfer
 * @val: used to store the read byte
 * @msg: message string
 *
 * Returns  - len if success else failure.
 */
static int mpu6050_i2c_read(struct device *dev, u8 addr, u8 reg, u8 len,
							u8 *val, u8 *msg)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_adapter *i2c_adap = client->adapter;
	struct i2c_msg msgs[2];
	int ret;

	if (val == NULL || i2c_adap == NULL)
		return -EINVAL;
	mdelay(1);

	msgs[0].addr = addr;
	msgs[0].flags = 0;	/* read */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = val;
	msgs[1].len = len;

	ret = i2c_transfer(i2c_adap, msgs, 2);
	if (ret < 2) {
		dev_err(&client->dev,
			"%s failed (%s, %d)\n", __func__, msg, ret);
		return ret;
	} else {
		return 0;
	}
}

static const struct mpu6050_bus_ops mpu6050_i2c_bops = {
	.bustype	= BUS_I2C,
#define MPU6050_BUSI2C     (0 << 4)
	.read		= mpu6050_i2c_read,
	.write		= mpu6050_i2c_write,
};

/**
 * mpu6050_i2c_probe - device detection callback
 * @client: i2c client of found device
 * @id: id match information
 *
 * The I2C layer calls us when it believes a sensor is present at this
 * address. Probe to see if this is correct and to validate the device.
 */

static int __devinit mpu6050_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct mpu6050_data *data = NULL;
	int ret = -1;

	data = kzalloc(sizeof(struct mpu6050_data), GFP_KERNEL);
	if (!data)
		goto err;
	data->client = client;
	i2c_set_clientdata(client, data);
	ret = mpu6050_init(data, &mpu6050_i2c_bops);
	if (ret) {
		dev_err(&client->dev, "%s failed\n", __func__);
		goto err;
	}
	return 0;
err:
	if (data != NULL)
		kfree(data);
	return ret;
}

/**
 * mpu6050_i2c_remove -	remove a sensor
 * @client: i2c client of sensor being removed
 */
static int __devexit mpu6050_i2c_remove(struct i2c_client *client)
{
	struct mpu6050_data *data = i2c_get_clientdata(client);

	mpu6050_exit(data);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
/**
 * mpu6050_i2c_suspend	- called on device suspend
 * @dev: device being suspended
 */
static int mpu6050_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_data *data = i2c_get_clientdata(client);

	mpu6050_suspend(data);

	return 0;
}

/**
 * mpu6050_i2c_resume	- called on device resume
 * @dev: device being resumed
 */
static int mpu6050_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_data *data = i2c_get_clientdata(client);

	mpu6050_resume(data);

	return 0;
}
#endif

static UNIVERSAL_DEV_PM_OPS(mpu6050_i2c_pm, mpu6050_i2c_suspend,
					mpu6050_i2c_resume, NULL);

static const struct i2c_device_id mpu6050_i2c_id[] = {
	{ "mpu6050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_i2c_id);

static struct i2c_driver mpu6050_i2c_driver = {
	.driver	= {
		.name	= "mpu6050",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &mpu6050_i2c_pm,
#endif
	},
	.probe		= mpu6050_i2c_probe,
	.remove		= __devexit_p(mpu6050_i2c_remove),
	.id_table	= mpu6050_i2c_id,
};

static int __init mpu6050_i2c_init(void)
{
	return i2c_add_driver(&mpu6050_i2c_driver);
}
module_init(mpu6050_i2c_init);

static void __exit mpu6050_i2c_exit(void)
{
	i2c_del_driver(&mpu6050_i2c_driver);
}
module_exit(mpu6050_i2c_exit)

MODULE_AUTHOR("Kishore Kadiyala <kishore.kadiyala@ti.com");
MODULE_DESCRIPTION("MPU6050 I2c Driver");
MODULE_LICENSE("GPL");
