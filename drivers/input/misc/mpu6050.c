/*
 * Implements driver for INVENSENSE MPU6050 Chip
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
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>

#include "mpu6050x.h"
#include <linux/input/mpu6050.h>

static int mpu6050_enable_sleep(struct mpu6050_data *data, int action)
{
	unsigned char val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
						1, &val, "Enable Sleep");
	if (action)
		val |= MPU6050_DEVICE_SLEEP_MODE;
	else
		val &= ~MPU6050_DEVICE_SLEEP_MODE;

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
							val, "Enable Sleep");
	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
						1, &val, "Enable Sleep");
	if (val & MPU6050_DEVICE_SLEEP_MODE)
		dev_err(data->dev, "Enable Sleep failed\n");
	return 0;
}

static int mpu6050_reset(struct mpu6050_data *data)
{
	unsigned char val = 0;

	/*
	 * Dummy read after bootup on I2c4 which was throwing timeout
	 * Applicable only for OMAP4460 on OMAP5 SEVM Board.
	 */

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
							1, &val, "Reset");
	/* Reset sequence */
	val |= MPU6050_DEVICE_RESET;
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
								val, "Reset");

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
							1, &val, "Reset");
	if (val & MPU6050_DEVICE_RESET) {
		dev_err(data->dev, "Reset failed\n");
		return val;
	}

	/*
	 * Reset of PWR_MGMT_1 reg puts the device in sleep mode,
	 * so disable the sleep mode
	 */
	mpu6050_enable_sleep(data, 0);

	return 0;
}

static int mpu6050_poweron(struct mpu6050_data *data)
{
	mpu6050_enable_sleep(data, 0);
	return 0;
}

static int mpu6050_poweroff(struct mpu6050_data *data)
{

	mpu6050_enable_sleep(data, 1);
	return 0;
}

void mpu6050_suspend(struct mpu6050_data *data)
{
	mutex_lock(&data->mutex);

	if (!data->suspended)
		mpu6050_poweroff(data);

	data->suspended = true;

	mutex_unlock(&data->mutex);

#ifdef	CONFIG_INPUT_MPU6050_ACCEL
	 mpu6050_accel_suspend(data->accel_data);
#endif

#ifdef	CONFIG_INPUT_MPU6050_GYRO
	 mpu6050_gyro_suspend(data->gyro_data);
#endif
}
EXPORT_SYMBOL(mpu6050_suspend);


void mpu6050_resume(struct mpu6050_data *data)
{
	mutex_lock(&data->mutex);

	if (data->suspended)
		mpu6050_poweron(data);

	data->suspended = false;

	mutex_unlock(&data->mutex);

#ifdef	CONFIG_INPUT_MPU6050_ACCEL
	 mpu6050_accel_resume(data->accel_data);
#endif

#ifdef	CONFIG_INPUT_MPU6050_GYRO
	 mpu6050_gyro_resume(data->gyro_data);
#endif
}
EXPORT_SYMBOL(mpu6050_resume);

int mpu6050_init(struct mpu6050_data *data, const struct mpu6050_bus_ops *bops)
{
	int error;

	if (!data->client->dev.platform_data) {
		dev_err(&data->client->dev, "platform data not found\n");
		error = -EINVAL;
		goto err_out;
	}

	/* if no IRQ return error */
	if (data->client->irq == 0) {
		dev_err(&data->client->dev, "Irq not set\n");
		error = -EINVAL;
		goto err_out;
	}
	data->dev = &data->client->dev;
	data->bus_ops = bops;
	data->pdata = data->client->dev.platform_data;
	data->irq = data->client->irq;
	mutex_init(&data->mutex);

	/* RESET MPU6050 */
	mpu6050_reset(data);

	/* TODO: Verify Product Revision for the MPU6050 */

	/*
	 * Initializing built in sensors on MPU.
	 * If init for individual sensor fails, report an error and move on.
	 */
	data->accel_data = mpu6050_accel_init(data);
	if (IS_ERR(data->accel_data))
		dev_err(data->dev, "MPU6050: mpu6050_accel_init failed\n");
	data->gyro_data = mpu6050_gyro_init(data);
	if (IS_ERR(data->gyro_data))
		dev_err(data->dev, "MPU6050: mpu6050_gyro_init failed\n");

	return 0;

err_out:
	return error;
}
EXPORT_SYMBOL(mpu6050_init);

void mpu6050_exit(struct mpu6050_data *data)
{

#ifdef CONFIG_INPUT_MPU6050_ACCEL
	mpu6050_accel_exit(data->accel_data);
#endif
#ifdef CONFIG_INPUT_MPU6050_GYRO
	mpu6050_gyro_exit(data->gyro_data);
#endif
}
EXPORT_SYMBOL(mpu6050_exit);

MODULE_AUTHOR("Kishore Kadiyala <kishore.kadiyala@ti.com");
MODULE_DESCRIPTION("MPU6050 Driver");
MODULE_LICENSE("GPL");
