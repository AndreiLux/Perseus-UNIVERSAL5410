/*
 * INVENSENSE MPU6050 driver
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

#ifndef _LINUX_MPU6050_H
#define _LINUX_MPU6050_H

/**
 * struct mpu6050_accel_platform_data - MPU6050 Accelerometer Platform data
 * @x_axis: X Axis accelerometer measurement
 * @y_axis: Y Axis accelerometer measurement
 * @z_axis: Z Axis accelerometer measurement
 * @fsr: full scale range
 * @hpf: high pass filter
 * @mode: supported modes were Free Fall/Motion Detection/Zero Motion detection.
 * @mode_threshold_val: Threshold value for the selected mode.
 * @mode_threshold_dur: Threshold duration for the selected mode.
 * @fifo_mode: Enabling fifo for Accelerometer.
 */

struct mpu6050_accel_platform_data {
	int x_axis;
	int y_axis;
	int z_axis;
	uint8_t fsr;
	uint8_t hpf;
	uint8_t ctrl_mode;
	uint8_t mode_thr_val;
	uint8_t mode_thr_dur;
	unsigned long irqflags;
};

/**
 * struct mpu6050_gyro_platform_data - MPU6050 Gyroscope Platform data
 * @x_axis: X Axis gyroscope measurement
 * @y_axis: Y Axis gyroscope measurement
 * @z_axis: Z Axis gyroscope measurement
 * @fsr: full scale range
 * @config: fsync and DLPF config for gyro
 * @fifo_enable: Enabling fifo for Gyroscope.
 */

struct mpu6050_gyro_platform_data {
	int x_axis;
	int y_axis;
	int z_axis;
	int fsr;
	uint8_t config;
	unsigned long irqflags;
};

/**
 * struct mpu6050_gyro_platform_data - MPU6050 Platform data
 * @aux_i2c_supply: Auxiliary i2c bus voltage supply level
 * @sample_rate_div: Samplerate of MPU6050
 * @config: fsync and DLPF config for accel
 * @Fifo: FIFO Mode enable/disable
 */

struct mpu6050_platform_data {
	uint8_t aux_i2c_supply;
	uint8_t sample_rate_div;
	uint8_t config;
	uint8_t fifo_mode;
	struct mpu6050_accel_platform_data mpu6050_accel;
	struct mpu6050_gyro_platform_data mpu6050_gyro;
};

#endif
