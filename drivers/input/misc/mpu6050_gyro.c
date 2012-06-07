/*
 * INVENSENSE MPU6050 Gyroscope driver
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

#include <linux/types.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/device.h>
#include "mpu6050x.h"
#include <linux/input/mpu6050.h>
#include <linux/i2c.h>

/* Put's the Gyroscope in Stand-by mode */
static int mpu6050_gyro_set_standby(struct mpu6050_gyro_data *data,
								int action)
{
	unsigned char val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_2,
							1, &val, "Gyro STBY");
	if (action)
		val |= (MPU6050_STBY_XG |  MPU6050_STBY_YG |  MPU6050_STBY_ZG);
	else
		val &= ~(MPU6050_STBY_XG |  MPU6050_STBY_YG |  MPU6050_STBY_ZG);

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
				MPU6050_REG_PWR_MGMT_2, val, "Gyro STBY");
	return 0;
}

static int mpu6050_gyro_reset(struct mpu6050_gyro_data *data)
{
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_SIGNAL_PATH_RESET, MPU6050_GYRO_SP_RESET,
							"Gyro SP-reset");
	mpu6050_gyro_set_standby(data, 0);
	return 0;
}

static int mpu6050_gyro_read_xyz(struct mpu6050_gyro_data *data)
{
	s16 datax, datay, dataz;
	u16 buffer[3];
	u8 val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_INT_STATUS,
						1, &val, "interrupt status");
	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_GYRO_XOUT_H,
						6, (u8 *)buffer, "gyro-x-y-z");

	datax = be16_to_cpu(buffer[0]);
	datay = be16_to_cpu(buffer[1]);
	dataz = be16_to_cpu(buffer[2]);

	input_report_abs(data->input_dev, ABS_X, datax);
	input_report_abs(data->input_dev, ABS_Y, datay);
	input_report_abs(data->input_dev, ABS_Z, dataz);
	input_sync(data->input_dev);

	return 0;
}

static int mpu6050_gyro_open(struct input_dev *input_dev)
{
	struct mpu6050_gyro_data *data = input_get_drvdata(input_dev);

	mutex_lock(&data->mutex);

	if (!data->suspended)
		mpu6050_gyro_set_standby(data, 0);

	data->opened = true;

	mutex_unlock(&data->mutex);

	return 0;
}

static void mpu6050_gyro_close(struct input_dev *input_dev)
{
	struct mpu6050_gyro_data *data = input_get_drvdata(input_dev);

	mutex_lock(&data->mutex);

	if (!data->suspended)
		mpu6050_gyro_set_standby(data, 1);

	data->opened = false;

	mutex_unlock(&data->mutex);
}

void mpu6050_gyro_suspend(struct mpu6050_gyro_data *data)
{
	mutex_lock(&data->mutex);

	if (!data->suspended && data->opened)
		mpu6050_gyro_set_standby(data, 1);

	data->suspended = true;

	mutex_unlock(&data->mutex);
}
EXPORT_SYMBOL(mpu6050_gyro_suspend);


void mpu6050_gyro_resume(struct mpu6050_gyro_data *data)
{
	mutex_lock(&data->mutex);

	if (data->suspended && data->opened)
		mpu6050_gyro_set_standby(data, 0);

	data->suspended = false;

	mutex_unlock(&data->mutex);
}
EXPORT_SYMBOL(mpu6050_gyro_resume);

/**
 * mpu6050_gyro_show_attr_xyz - Reports x,y,z axis events to input device
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_gyro_show_attr_xyz(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_gyro_data *data = mpu_data->gyro_data;

	return  mpu6050_gyro_read_xyz(data);
}

static DEVICE_ATTR(xyz, S_IWUSR | S_IRUGO,
				mpu6050_gyro_show_attr_xyz, NULL);

static struct attribute *mpu6050_gyro_attrs[] = {
	&dev_attr_xyz.attr,
	NULL,
};

static struct attribute_group mpu6050_gyro_attr_group = {
	.attrs = mpu6050_gyro_attrs,
};

struct mpu6050_gyro_data *mpu6050_gyro_init(const struct mpu6050_data *mpu_data)
{
	const struct mpu6050_gyro_platform_data *pdata;
	struct mpu6050_gyro_data *gyro_data;
	struct input_dev *input_dev;
	int error;

	pdata = &mpu_data->pdata->mpu6050_gyro;
	gyro_data = kzalloc(sizeof(struct mpu6050_gyro_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!gyro_data || !input_dev) {
		error = -ENOMEM;
		goto err_free_mem;
	}
	gyro_data->dev = mpu_data->dev;
	gyro_data->input_dev = input_dev;
	gyro_data->bus_ops = mpu_data->bus_ops;
	mutex_init(&gyro_data->mutex);


	/*TODO: Mode & FSR Range Check */

	input_dev->name = "mpu6050-gyroscope";
	input_dev->id.bustype = mpu_data->bus_ops->bustype;
	input_dev->open = mpu6050_gyro_open;
	input_dev->close = mpu6050_gyro_close;

	 __set_bit(EV_ABS, input_dev->evbit);

	input_set_abs_params(input_dev, ABS_X, MPU6050_GYRO_MIN_VALUE,
				MPU6050_GYRO_MAX_VALUE, pdata->x_axis, 0);
	input_set_abs_params(input_dev, ABS_Y, MPU6050_GYRO_MIN_VALUE,
				MPU6050_GYRO_MAX_VALUE, pdata->y_axis, 0);
	input_set_abs_params(input_dev, ABS_Z, MPU6050_GYRO_MIN_VALUE,
				MPU6050_GYRO_MAX_VALUE, pdata->z_axis, 0);

	input_set_drvdata(input_dev, gyro_data);

	/*  Reset Gyroscope signal path */
	mpu6050_gyro_reset(gyro_data);

	error = input_register_device(gyro_data->input_dev);
	if (error) {
		dev_err(gyro_data->dev, "Unable to register input device\n");
		goto err_free_mem;
	}
	error = sysfs_create_group(&mpu_data->client->dev.kobj,
					&mpu6050_gyro_attr_group);
	return gyro_data;

err_free_mem:
	input_free_device(input_dev);
	kfree(gyro_data);
	return ERR_PTR(error);
}
EXPORT_SYMBOL(mpu6050_gyro_init);

void mpu6050_gyro_exit(struct mpu6050_gyro_data *data)
{
	input_unregister_device(data->input_dev);
	kfree(data);
}
EXPORT_SYMBOL(mpu6050_gyro_exit);

MODULE_AUTHOR("Kishore Kadiyala <kishore.kadiyala@ti.com");
MODULE_DESCRIPTION("MPU6050 I2c Driver");
MODULE_LICENSE("GPL");
