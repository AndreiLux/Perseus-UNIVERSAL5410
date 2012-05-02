/*
 * INVENSENSE MPU6050 Accelerometer driver
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
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/input/mpu6050.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "mpu6050x.h"


/*MPU6050 full scale range types*/
static int mpu6050_grange_table[4] = {
	2000,
	4000,
	8000,
	16000,
};

/**
 * mpu6050_accel_set_standby - Put's the axis of acceleromter in standby or not
 * @mpu6050_accel_data: accelerometer data
 * @action: to put into standby mode or not
 *
 * Returns '0' on success.
 */
static int mpu6050_accel_set_standby(struct mpu6050_accel_data *data,
								int action)
{
	unsigned char val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_2,
							1, &val, "Accel STBY");
	if (action)
		val |= (MPU6050_STBY_XA |  MPU6050_STBY_YA |  MPU6050_STBY_ZA);
	else
		val &= ~(MPU6050_STBY_XA |  MPU6050_STBY_YA |  MPU6050_STBY_ZA);

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
			MPU6050_REG_PWR_MGMT_2, val, "Accel STBY");
	return 0;
}

/**
 * mpu6050_accel_reset - Reset's the signal path of acceleromter
 * @mpu6050_accel_data: accelerometer data
 *
 * Returns '0' on success.
 */
static int mpu6050_accel_reset(struct mpu6050_accel_data *data)
{
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_SIGNAL_PATH_RESET, MPU6050_ACCEL_SP_RESET,
							"Accel SP-reset");
	mpu6050_accel_set_standby(data, 0);
	return 0;
}

/**
 * mpu6050_data_decode_mg - Data in 2's complement, convert to mg
 * @mpu6050_accel_data: accelerometer data
 * @datax: X axis data value
 * @datay: Y axis data value
 * @dataz: Z axis data value
 */
static void mpu6050_data_decode_mg(struct mpu6050_accel_data *data,
				s16 *datax, s16 *datay, s16 *dataz)
{
	u8 fsr;
	int range;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_CONFIG, 1, &fsr, "full scale range");
	range = mpu6050_grange_table[(fsr >> 3)];
	/* Data in 2's complement, convert to mg */
	*datax = (*datax * range) / MPU6050_ABS_READING;
	*datay = (*datay * range) / MPU6050_ABS_READING;
	*dataz = (*dataz * range) / MPU6050_ABS_READING;
}

static irqreturn_t mpu6050_accel_thread_irq(int irq, void *dev_id)
{
	struct mpu6050_accel_data *data = dev_id;
	s16 datax, datay, dataz;
	u16 buffer[3];
	u8 val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_INT_STATUS,
						1, &val, "interrupt status");
	dev_dbg(data->dev, "MPU6050 Interrupt status Reg=%x\n", val);
	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_XOUT_H, 6, (u8 *)buffer, "Accel-x-y-z");

	datax = be16_to_cpu(buffer[0]);
	datay = be16_to_cpu(buffer[1]);
	dataz = be16_to_cpu(buffer[2]);

	mpu6050_data_decode_mg(data, &datax, &datay, &dataz);

	input_report_abs(data->input_dev, ABS_X, datax);
	input_report_abs(data->input_dev, ABS_Y, datay);
	input_report_abs(data->input_dev, ABS_Z, dataz);
	input_sync(data->input_dev);

	return IRQ_HANDLED;
}

static int mpu6050_accel_open(struct input_dev *input_dev)
{
	struct mpu6050_accel_data *data = input_get_drvdata(input_dev);

	mutex_lock(&data->mutex);

	if (!data->suspended)
		mpu6050_accel_set_standby(data, 0);

	data->opened = true;

	mutex_unlock(&data->mutex);

	return 0;
}

static void mpu6050_accel_close(struct input_dev *input_dev)
{
	struct mpu6050_accel_data *data = input_get_drvdata(input_dev);

	mutex_lock(&data->mutex);
	if (!data->suspended)
		mpu6050_accel_set_standby(data, 1);
	data->opened = false;
	mutex_unlock(&data->mutex);
}

void mpu6050_accel_suspend(struct mpu6050_accel_data *data)
{
	mutex_lock(&data->mutex);
	if (!data->suspended && data->opened)
		mpu6050_accel_set_standby(data, 1);
	data->suspended = true;
	mutex_unlock(&data->mutex);
}
EXPORT_SYMBOL(mpu6050_accel_suspend);


void mpu6050_accel_resume(struct mpu6050_accel_data *data)
{
	mutex_lock(&data->mutex);
	if (data->suspended && data->opened)
		mpu6050_accel_set_standby(data, 0);
	data->suspended = false;
	mutex_unlock(&data->mutex);
}
EXPORT_SYMBOL(mpu6050_accel_resume);

/**
 * mpu6050_accel_show_attr_modedur - sys entry to show threshold duration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_modedur(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t modedur;
	uint8_t reg = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_DUR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_DUR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_DUR;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, reg, 1,
					&modedur, "Attr mode Duration");
	return sprintf(buf, "%x\n", modedur);
}

/**
 * mpu6050_accel_store_attr_modedur - sys entry to set threshold duration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_modedur(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;
	uint8_t reg = 0;

	error = strict_strtoul(buf, 16, &val);
	if (error)
		return error;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_DUR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_DUR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_DUR;

	mutex_lock(&data->mutex);
	disable_irq(data->irq);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, reg, val,
						"Attr mode Duration");
	enable_irq(data->irq);
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_modethr - sys entry to show threshold value.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_modethr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t modethr;
	uint8_t reg = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_THR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_THR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_THR;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, reg, 1,
					&modethr, "Attr mode threshold");
	return sprintf(buf, "%x\n", modethr);
}

/**
 * mpu6050_accel_store_attr_modethr - sys entry to set threshold value.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_modethr(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;
	uint8_t reg = 0;

	error = strict_strtoul(buf, 16, &val);
	if (error)
		return error;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_THR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_THR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_THR;

	mutex_lock(&data->mutex);
	disable_irq(data->irq);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, reg, val,
						"Attr mode threshold");
	enable_irq(data->irq);
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_confmode - sys entry to show accel configration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_confmode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t mode;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	mutex_lock(&data->mutex);
	mode = data->mode;
	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d\n", mode);
}

/**
 * mpu6050_accel_store_attr_confmode - sys entry to set accel configuration
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_confmode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error = -1;

	error = strict_strtoul(buf, 10, &val);
	if (error)
		return error;

	if (val < FREE_FALL_MODE || val > ZERO_MOT_DET_MODE)
		return -EINVAL;

	mutex_lock(&data->mutex);
	data->mode = (enum accel_op_mode) val;
	if (data->mode == FREE_FALL_MODE)
		val = 1 << 7;
	else if (data->mode == MOTION_DET_MODE)
		val = 1 << 6;
	else
		val = 1 << 5;
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_INT_ENABLE, val,
								"Attr conf mode");

	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_enable - sys entry to show accel enable state.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds enable state value.
 *
 * Returns 'enable' state.
 */
static ssize_t mpu6050_accel_show_attr_enable(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	int val;

	val = data->enabled;
	if (val < 0)
		return val;

	return sprintf(buf, "%d\n", val);
}

/**
 * mpu6050_accel_store_attr_enable - sys entry to set accel enable state.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds enable state value.
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_enable(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	if (val < 0 || val > 1)
		return -EINVAL;

	if (val) {
		if (!data->suspended)
			mpu6050_accel_set_standby(data, 0);
		data->opened = true;
		data->enabled = 1;
	} else {
		if (!data->suspended)
			mpu6050_accel_set_standby(data, 1);
		data->opened = false;
		data->enabled = 0;
	}
	return count;
}

/**
 * mpu6050_accel_store_attr_ast - sys entry to set the axis for self test
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_ast(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	uint8_t reg;
	int error = -1;

	error = strict_strtoul(buf, 10, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	disable_irq(data->irq);
	if (val == 1)
		reg = (0x1 << 7);
	else if (val == 2)
		reg = (0x1 << 6);
	else if (val == 3)
		reg = (0x1 << 5);
	else
		return error;

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
							reg, "Accel Self Test");
	enable_irq(data->irq);
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_hpf - sys entry to show DHPF configration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_hpf(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t hpf;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
			MPU6050_REG_ACCEL_CONFIG, 1, &hpf, "Attr show fsr");
	return sprintf(buf, "%d\n", hpf);
}

/**
 * mpu6050_accel_store_attr_ast - sys entry to set DHPF
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_hpf(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 16, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	disable_irq(data->irq);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
							val, "Attr store fsr");
	enable_irq(data->irq);
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_hpf - sys entry to show full scale range
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_fsr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t fsr;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_CONFIG, 1, &fsr, "Attr show fsr");
	return sprintf(buf, "%d\n", (fsr >> 3));
}

/**
 * mpu6050_accel_store_attr_ast - sys entry to set the full scale range
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_fsr(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 16, &val);
	if (error)
		return error;
	mutex_lock(&data->mutex);
	disable_irq(data->irq);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
						(val << 3), "Attr store fsr");
	enable_irq(data->irq);
	mutex_unlock(&data->mutex);
	return count;
}

static DEVICE_ATTR(fsr, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_fsr,
		mpu6050_accel_store_attr_fsr);

static DEVICE_ATTR(hpf, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_hpf,
		mpu6050_accel_store_attr_hpf);

static DEVICE_ATTR(ast, S_IWUSR | S_IRUGO, NULL,
		mpu6050_accel_store_attr_ast);

static DEVICE_ATTR(confmode, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_confmode,
		mpu6050_accel_store_attr_confmode);

static DEVICE_ATTR(modedur, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_modedur,
		mpu6050_accel_store_attr_modedur);

static DEVICE_ATTR(modethr, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_modethr,
		mpu6050_accel_store_attr_modethr);
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_enable,
		mpu6050_accel_store_attr_enable);


static struct attribute *mpu6050_accel_attrs[] = {
	&dev_attr_fsr.attr,
	&dev_attr_hpf.attr,
	&dev_attr_ast.attr,
	&dev_attr_confmode.attr,
	&dev_attr_modedur.attr,
	&dev_attr_modethr.attr,
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group mpu6050_accel_attr_group = {
	.attrs = mpu6050_accel_attrs,
};

struct mpu6050_accel_data *mpu6050_accel_init(
			const struct mpu6050_data *mpu_data)
{
	const struct mpu6050_accel_platform_data *pdata;
	struct mpu6050_accel_data *accel_data;
	struct input_dev *input_dev;
	int error;
	int reg_thr, reg_dur;
	u8 intr_mask;
	u8 val;

	/* if no IRQ return error */
	if (!mpu_data->irq) {
		dev_err(mpu_data->accel_data->dev, "MPU6050 Accelerometer Irq not assigned\n");
		error = -EINVAL;
		goto err_out;
	}

	pdata = &mpu_data->pdata->mpu6050_accel;
	accel_data = kzalloc(sizeof(struct mpu6050_accel_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!accel_data || !input_dev) {
		error = -ENOMEM;
		goto err_free_mem;
	}
	accel_data->dev = mpu_data->dev;
	accel_data->input_dev = input_dev;
	accel_data->bus_ops = mpu_data->bus_ops;
	accel_data->irq = mpu_data->irq;
	accel_data->enabled = 0;
	mutex_init(&accel_data->mutex);

	/* Configure the init values from platform data */
	MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_CONFIG, (pdata->fsr << 3), "Init fsr");
	MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_CONFIG, pdata->hpf, "Init hpf");
	accel_data->mode = pdata->ctrl_mode;
	if (pdata->ctrl_mode == FREE_FALL_MODE) {
		reg_thr = MPU6050_REG_ACCEL_FF_THR;
		reg_dur = MPU6050_REG_ACCEL_FF_DUR;
		intr_mask = MPU6050_FF_INT;
	} else if (pdata->ctrl_mode == MOTION_DET_MODE) {
		reg_thr = MPU6050_REG_ACCEL_MOT_THR;
		reg_dur = MPU6050_REG_ACCEL_MOT_DUR;
		intr_mask = MPU6050_MOT_INT;
	} else {
		/* SET TO ZERO_MOT_DET_MODE by default */
		reg_thr = MPU6050_REG_ACCEL_MOT_THR;
		reg_dur = MPU6050_REG_ACCEL_MOT_DUR;
		intr_mask = MPU6050_ZMOT_INT;
	}
	MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
			reg_thr, pdata->mode_thr_val, "Init thr");
	MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
			reg_dur, pdata->mode_thr_dur, "Init dur");

	input_dev->name = "mpu6050-accelerometer";
	input_dev->id.bustype = mpu_data->bus_ops->bustype;
	input_dev->open = mpu6050_accel_open;
	input_dev->close = mpu6050_accel_close;

	 __set_bit(EV_ABS, input_dev->evbit);

	input_set_abs_params(input_dev, ABS_X,
			-mpu6050_grange_table[pdata->fsr],
			mpu6050_grange_table[pdata->fsr], pdata->x_axis, 0);
	input_set_abs_params(input_dev, ABS_Y,
			-mpu6050_grange_table[pdata->fsr],
			mpu6050_grange_table[pdata->fsr], pdata->y_axis, 0);
	input_set_abs_params(input_dev, ABS_Z,
			-mpu6050_grange_table[pdata->fsr],
			mpu6050_grange_table[pdata->fsr], pdata->z_axis, 0);

	input_set_drvdata(input_dev, accel_data);

	/* Reset Accelermter signal path */
	mpu6050_accel_reset(accel_data);
	error = gpio_request_one(accel_data->irq, GPIOF_IN, "sensor");
	if (error) {
		dev_err(accel_data->dev, "sensor: gpio request failure\n");
		goto err_free_gpio;
	}
	error = request_threaded_irq(gpio_to_irq(accel_data->irq), NULL,
			mpu6050_accel_thread_irq,
			pdata->irqflags | IRQF_ONESHOT,
			"mpu6050_accel_irq", accel_data);
	if (error) {
		dev_err(accel_data->dev, "request_threaded_irq failed\n");
		goto err_free_mem;
	}

	error = input_register_device(accel_data->input_dev);
	if (error) {
		dev_err(accel_data->dev, "Unable to register input device\n");
		goto err_free_irq;
	}

	error = sysfs_create_group(&mpu_data->client->dev.kobj,
					&mpu6050_accel_attr_group);
	if (error) {
		dev_err(&mpu_data->client->dev,
			"failed to create sysfs entries\n");
		goto err_free_irq;
	}

	/* Enable interrupt for Defined mode */
	MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_INT_ENABLE, intr_mask, "Enable Intr");
	/* Clear interrupts for Defined mode */
	MPU6050_READ(accel_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_INT_STATUS, 1, &val, "Clear Isr");

	/* Disable Accelerometer by default */
	if (!accel_data->suspended)
		mpu6050_accel_set_standby(accel_data, 1);
	accel_data->opened = false;

	return accel_data;

err_free_gpio:
	gpio_free(mpu_data->client->irq);
err_free_irq:
	free_irq(accel_data->irq, accel_data);
err_free_mem:
	input_free_device(input_dev);
	kfree(accel_data);
err_out:
	return ERR_PTR(error);
}
EXPORT_SYMBOL(mpu6050_accel_init);

void mpu6050_accel_exit(struct mpu6050_accel_data *data)
{
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data);
}
EXPORT_SYMBOL(mpu6050_accel_exit);

MODULE_AUTHOR("Kishore Kadiyala <kishore.kadiyala@ti.com");
MODULE_DESCRIPTION("MPU6050 I2c Driver");
MODULE_LICENSE("GPL");
